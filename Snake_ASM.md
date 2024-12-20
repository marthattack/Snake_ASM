# Snake_ASM

## Installation

1. Télécharger le dossier compressé (ou cloner tout le repo) :
   
   ```bash
    git clone https://github.com/marthattack/Snake_ASM.git
   ```

2. Décompresser le dossier 

3. Dans un terminal, accéder `/Projet/mini-risc-freertos/`

4. Compiler et éxecuter `main.c` avec le compilateur adéquat :
   
   ```bash
   make exec
   ```

5. Vous pouvez jouer à Snake sur FreeRTOS.



## Fonctionnement

La fonction `int main()` met en place les interruptions clavier et temporelles, ainsi qu'un mutex,  puis crée les deux processus qui s'occuperont de gérèr la logique du jeu `void logique()` et l'affichage `void video`. 

### Structure des données :

Le plateau de jeu est représenté par un tableau d'entiers à une dimension, dans lequel on stocke successivement les lignes. Ainsi, les coordonées $(x,y)$ correspondent à la case $i = x*NombreCases + y$ ou $NombreCases$ est la largeur du tableau (`SCREEN_WIDTH` dans notre programme). 

Dans ce tableau, les entiers sont soit 0 (rien), soit -1 (pomme), soit des entiers entre 1 et 5 qui représentent une partie du serpent (tête, queue, corps) et qui donnent la position de la partie suivante du serpent.



Nous utilisons aussi une file pour transmettre les interruptions clavier au processus qui éxecute la logique du jeu, et un mutex pour éviter les problèmes lors de l'accès concurrent au tableau par les processus d'affichage et de logique.  Tous deux sont initialisés dans le main.





## 


