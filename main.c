#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "harvey_platform.h"
#include "minirisc.h"
#include "uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "xprintf.h"
#include "queue.h"


#define SCREEN_WIDTH  24
#define SCREEN_HEIGHT 24

//création du mutex sur le tableau
static SemaphoreHandle_t array_mutex = NULL;

//déclaration globale du terrain
int* terrain; //création du tableau terrain
int fin = 0; //signale la fin à tous les processus
static QueueHandle_t command_queue; //la queue à une longueur 1 car on ne veut pas prendre plus de 1 commande par step
//initialisation de variables additionelles
int command = 2;
int score  = 0;

//partie vidéo
static uint32_t frame_buffer[480 * 480];
volatile uint32_t color = 0x00ff0000;



uint32_t make_color(uint8_t red, uint8_t green, uint8_t blue) {
    return ((uint32_t)red << 16) | ((uint32_t)green << 8) | (uint32_t)blue;
}

void init_video()
{
	memset(frame_buffer, 0, sizeof(frame_buffer)); // clear frame buffer to black
	VIDEO->WIDTH  = 480;
	VIDEO->HEIGHT = 480;
	VIDEO->DMA_ADDR = frame_buffer;
	VIDEO->CR = VIDEO_CR_IE | VIDEO_CR_EN;
}


void video(void* arg)
{
	(void)arg;
	while (!fin)
	{

		while((VIDEO->SR>>1)%2 != 1)
		{}
		//début du rafraichissement
		VIDEO->SR = 0;
		for (int i = 0; i < SCREEN_WIDTH*SCREEN_HEIGHT; i++)
		{
			xSemaphoreTake(array_mutex, portMAX_DELAY);
			//DEBUT SECTION CRITIQUE
			if (terrain[i] > 0)
			{
				for (int x = 0; x < 20; x++)
				{
					for (int y = 0; y < 20; y++)
					{
						VIDEO->DMA_ADDR[x*VIDEO->WIDTH + y] = make_color(0,255,0);
					}
				}

			}
			else if (terrain[i] == -1)
			{
				for (int x = 0; x < 20; x++)
				{
					for (int y = 0; y < 20; y++)
					{
						VIDEO->DMA_ADDR[x*VIDEO->WIDTH + y] = make_color(255,0,0);
					}
				}
			}
			else
			{
				for (int x = 0; x < 20; x++)
				{
					for (int y = 0; y < 20; y++)
					{
						VIDEO->DMA_ADDR[x*VIDEO->WIDTH + y] = make_color(0,0,0);
					}
				}
			}

			i++;
		}

	}
	free((uint32_t*)VIDEO->DMA_ADDR);


	//return 0;
}

//fin de partie vidéo
int vx = 1;
int vy = 0;

void keyboard_interrupt_handler()
{
	uint32_t kdata;
	while (KEYBOARD->SR & KEYBOARD_SR_FIFO_NOT_EMPTY) {
		kdata = KEYBOARD->DATA;
		if (kdata & KEYBOARD_DATA_PRESSED) {
			xprintf("key code: %d\n", KEYBOARD_KEY_CODE(kdata));
			switch (KEYBOARD_KEY_CODE(kdata)) {
				case 113: // Q
					minirisc_halt();
					break;
				case 37:
					//left arrow
					if (vx != -1)
					{
						vx = -1;
						vy = 0;
						command = 4;
					}
					break;
				case 38:
					//up arrow
					if(vy!=1)
					{
						vx = 0;
						vy = 1;
						command = 3;
					}
					break;
				case 39:
					//right arrow
					if (vx != 1)
					{
						vx = 1;
						vy = 0;
						command = 2;
					}
					break;
				case 40:
					//down arrow
					if (vy != -1)
					{
						vx = 0;
						vy = -1;
						command = 1;
					}
					break;
			}
			BaseType_t xHigherPriorityTaskWoken = pdFALSE;
			char c;
			xQueueSendFromISR(command_queue, &c, &xHigherPriorityTaskWoken);
		}
	}
}








void logique(void * arg)
{
	(void)arg;
	


	//configuration du timer, toutes les 0.5s
	TIMER->ARR = 500;

	TIMER->CR |= TIMER_CR_EN | TIMER_CR_IE;
	KEYBOARD->CR |= KEYBOARD_CR_IE;

	minirisc_enable_interrupt(TIMER_INTERRUPT);

	minirisc_enable_global_interrupts();

	
	/*Le terrain est géré de la façon suivante :
	-Chaque case du tableau terrain représente une case du terrain
	-l'accés à une case du tableau se fait avec la formule tableau[x][y] = tableau[x*SCREEN_WIDTH + y]
	-Cette case ne peut prendre que certaines valeurs :
		- 0 si la case est vie, elle est donc libre et le serpent peut y avancer
		- -1 si la case contient une pomme
		- 1, 2, 3, 4 ou 5 si la case contient une des écailles du serpent
	- la valeur d'une des écailles indique la direction de la prochaine écaille du serpent
	- ainsi, si le serpent avance, on fait avancer le pointeur de la queue vers l'écaille suivante et on supprime la dernière écaille
	
	Significatation des valeurs des écailles :
	.1 la prochaine écaille est en bas
	.2 la prochaine écaille est à droite
	.3 la prochaine écaille est en haut
	.4 la prochaine écaille est à gauche
	.5 indique que cette case est la tête du serpent et qu'on ne connait pas encore la prochaine écaille
	*/


	//initialisation des 3 premières écailles
	int x = SCREEN_WIDTH/2;
	int y = SCREEN_HEIGHT/2;


	xSemaphoreTake(array_mutex, portMAX_DELAY);
	//DEBUT SECTION CRITIQUE
	terrain[x*SCREEN_WIDTH + y] = 5;
	terrain[x*SCREEN_WIDTH + y-1] = 2;
	terrain[x*SCREEN_WIDTH + y-2] = 2;


	//initialisation des pointeurs de tête et de queue
	int pQueue = x*SCREEN_WIDTH + y-2;
	int pHead = x*SCREEN_WIDTH + y;

	//initialisation de la pomme, elle est traditionellement en face du serpent
	// /!\ Cette ligne peut générer des erreurs pour de petits écrans.
	terrain[x*SCREEN_WIDTH + 3*SCREEN_WIDTH/4] = -1;
	//FIN SECTION CRITIQUE
	xSemaphoreGive(array_mutex);
	
	while (!fin) {
		//on attend l'interrupt du timer
		vTaskDelay(MS2TICKS(1000));

		//lire l'instruction dans la pile
		if (uxQueueMessagesWaiting(command_queue) > 0)
		{
			while(xQueueReceive(command_queue, &command_queue, portMAX_DELAY) == pdFALSE) {printf("Notr reading anything from the queue, but queue not empty, trying again...\n");}
			//la commande est donc maintenant dans la variable commande
		}
		//si pas d'instruction, l'instruction est la même que la précédente (ne pas reset la valeur)

		//dans tous les cas, faire avancer le pointeur de tête et remplir la case de la tête précédente avec la bonne direction

		xSemaphoreTake(array_mutex, portMAX_DELAY);
		terrain[pHead] = command; //remplir la case, SECTION CRITIQUE

		//update le pointeur
		switch (command)
		{
			case 1:
				pHead += 1; //les ordonnées sont descendantes
				break;
			case 2:
				pHead += SCREEN_WIDTH;
				break;
			case 3:
				pHead -= 1;
				break;
			case 4:
				pHead -= SCREEN_WIDTH;
				break;
		}

		//signaler toute collision en mettant fin au jeu (par exemple en mettant la première case du tableau à -2)
		// si x > screen width ou < 0, y > screen heigth ou < 0...

		x = pHead/SCREEN_WIDTH;
		y = pHead%SCREEN_WIDTH;

		fin = ((x >= 0) && (x < SCREEN_WIDTH) && (y >= 0) && (y < SCREEN_HEIGHT));
		
		//si pas de collision, vérifier si une pomme est sur la case
		if (!fin)
		{
			if (terrain[pHead] == -1) //si on a une pomme, pas section critique, on ne fait que lire
			{
				score += 1;//on augmente le score

				x = rand()%SCREEN_WIDTH;
				y = rand()%SCREEN_HEIGHT;

				while (terrain[x*SCREEN_WIDTH + y] > 0) //si on tombe sur une pomme ou le serpent, pas de section critique, on ne fait que lire
				{
					//on regénére une nouvelle position jusqu'a tomber sur une case libre
					x = rand()%SCREEN_WIDTH;
					y = rand()%SCREEN_HEIGHT;
				}

			}
			//si pas de pomme, modifier le pointeur de queue
			else
			{
				x = terrain[pQueue]; //j'utilise x comme variable temporaire vu qu'elle existe...
				switch (x) //on change le pointeur de queue
				{
					case 1:
						pQueue += 1; //les ordonnées sont descendantes
						break;
					case 2:
						pQueue += SCREEN_WIDTH; //on rappel que le tableau est en réalité à une dimension
						break;
					case 3:
						pQueue-= 1;
						break;
					case 4:
						pQueue -= SCREEN_WIDTH;
						break;
				}
				terrain[pQueue] = 0; //SECTION CRITIQUE

			}
		}
		VIDEO->SR = 1;
		xSemaphoreGive(array_mutex);
		
	}

	//return 0;
}


int main()
{
	init_uart();
	array_mutex = xSemaphoreCreateMutex();
	terrain = malloc(sizeof(int)*SCREEN_WIDTH*SCREEN_HEIGHT);
	command_queue = xQueueCreate(1, sizeof(int));
	init_video();

	xTaskCreate(logique, "logique", 1024, NULL, 1, NULL);
	xTaskCreate(video,  "video",  1024, NULL, 1, NULL);
	vTaskStartScheduler();

	return 0;
}

