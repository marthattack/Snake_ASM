#include <string.h>
#include "minirisc.h"
#include "harvey_platform.h"
#include "xprintf.h"
#include "snake.h"

#define SCREEN_WIDTH  24
#define SCREEN_HEIGHT 24

static uint32_t frame_buffer[SCREEN_WIDTH * SCREEN_HEIGHT];
volatile uint32_t color = 0x00ff0000;

volatile int timer_event;


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
					}
				case 38:
					//up arrow
					if(vy!=1)
					{
						vx = 0;
						vy = 1;
					}
				case 39:
					//right arrow
					if (vx != 1)
					{
						vx = 1;
						vy = 0;
					}
				case 40:
					//down arrow
					if (vy != -1)
					{
						vx = 0;
						vy = -1;
					}
			}
		}
	}
}


//handler de l'interrupt du timer
void timer_interrupt_handler()
{
	/* Cear the bit using a logical and with the complement of the bit */
	TIMER->SR &= ~TIMER_SR_UEF;
	timer_event = 1;
}



static QueueHandle_t command_queue = xQueueCreate(1, sizeof(int)); //la queue à une longueur 1 car on ne veut pas prendre plus de 1 commande par step

int main()
{
	init_video();


	//configuration du timer, toutes les 0.5s
	TIMER->ARR = 500;

	TIMER->CR |= TIMER_CR_EN | TIMER_CR_IE;

	minirisc_enable_interrupt(TIMER_INTERRUPT);

	minirisc_enable_global_interrupts();

	terrain = malloc(sizeof(int)*SCREEN_WIDTH*SCREEN_HEIGTH); //création du tableau terrain
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
	int y = SCREEN_HEIGTH/2;

	terrain[x*SCREEN_WIDTH + y] = 5;
	terrain[x*SCREEN_WIDTH + y-1] = 2;
	terrain[x*SCREEN_WIDTH + y-2] = 2;

	//initialisation des pointeurs de tête et de queue
	int pQueue = x*SCREEN_WIDTH + y-2;
	int pHead = x*SCREEN_WIDTH + y;

	//initialisation de la pomme, elle est traditionellement en face du serpent
	// /!\ Cette ligne peut générer des erreurs pour de petits écrans.
	terrain[x*SCREEN_WIDTH + 3*SCREEN_WIDTH/4] = -1;


	//initialisation de variables additionelles
	bool fin = false;
	int command = 2;
	int score  = 0;
	
	while (!fin) {
		//on attend l'interrupt du timer
		minirisc_wait_for_interrupt();

		//lire l'instruction dans la pile
		if (uxQueueMessagesWaiting( QueueHandle_t xQueue ) > 0)
		{
			while(xQueueReceive(uart_rx_queue, &command, portMAX_DELAY) == pdFALSE) {printf("Notr reading anything from the queue, but queue not empty, trying again...\n");}
			//la commande est donc maintenant dans la variable commande
		}
		//si pas d'instruction, l'instruction est la même que la précédente (ne pas reset la valeur)

		//dans tous les cas, faire avancer le pointeur de tête et remplir la case de la tête précédente avec la bonne direction
		terrain[pHead] = command; //remplir la case

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
			if (terrain[pHead] == -1) //si on a une pomme
			{
				score += 1;//on augmente le score

				x = rand()%SCREEN_WIDTH;
				y = rand()%SCREEN_HEIGHT;

				while (terrain[x*SCREEN_WIDTH + y] > 0) //si on tombe sur une pomme ou le serpent
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
						pQueue += SCREEN_WIDTH;
						break;
					case 3:
						pQueue-= 1;
						break;
					case 4:
						pQueue -= SCREEN_WIDTH;
						break;
				}
				terrain[pQueue] = 0;

			}
		}
		
	}

	return 0;
}


