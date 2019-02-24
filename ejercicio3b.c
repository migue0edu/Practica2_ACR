#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#define PRODS 100
#define SECCS 2

char seccionCritica[SECCS][5];
int semId;
int totalConsumos;

void miWait(int idsem){
    struct sembuf opciones;
    opciones.sem_flg = SEM_UNDO;
    opciones.sem_num = idsem;
    opciones.sem_op = -1;
    semop(semId,&opciones,1);
}

void miSignal(int idsem){
    struct sembuf opciones;
    opciones.sem_flg = SEM_UNDO;
    opciones.sem_num = idsem;
    opciones.sem_op = 1;
    semop(semId,&opciones,1);
}

void eliminarSemaforo(){
    if(semctl(semId, 0, IPC_RMID) == -1){
        perror("No se pudo eliminar el semáforo.");
    }
}

void *lectura(void *param){
	int *id = (int *)param;
	printf("+++++++++++%d++++++++++++\n", *id);
     while(totalConsumos > 0){
		miWait(1);
        for(int i=0; i < SECCS; i++){
            //Preguntar por el estado del semáforo de la 1er pos. de la sección critica.
            if(semctl(semId, (i*2)+3, GETVAL) == 1) {
                miWait((i*2)+3); //Decrementar semáforo.
                printf("%d--> <%d> Leido de [%d]: %s \n",totalConsumos,*id, i, seccionCritica[i]);
                totalConsumos--;
                miSignal((i*2)+2);//Incrementa semáforo para lectura.
                miSignal(0);
                continue;
     	    }
        }
	}
    
    return NULL;

}
//[ P, C, p1, c1, p2, c2 ]#define NUMSEM 6
void *escritura(void *param){
	int *id = (int *)param;
	printf("-----------%d------------\n", *id);
    int datosEscritos = PRODS;
     
     while(datosEscritos > 0){
		miWait(0);
     	//Preguntar por el estado del semáforo de la 1er pos. de la sección critica.
		for(int i=0; i < SECCS; i++){
			if(semctl(semId, (i*2)+2, GETVAL) == 1) {
				miWait((i*2)+2); //Decrementar semáforo.
				strcpy(seccionCritica[i], "ABCD");
				if(*id == 0 )
					strcpy(seccionCritica[i], "0123");
				printf("%d.- { %d } Escrito en [0]: %s \n",PRODS - datosEscritos,*id, seccionCritica[i]);
				datosEscritos--;
				miSignal((i*2)+3);//Incrementa semáforo para lectura.
				miSignal(1);
				break;
			}
		}
     }  
    return NULL;
}


int main(int argc, char const *argv[]){
    int numProds;
    int numCons;
	int *arrIdProds;
    int *arrIdCons;

    if(argc < 3){
        printf("Uso: <Productores> <Consumidores>");
        return -1;
    }
    //Obtner desde los argumentos el numero de productores y consumidores.
    numProds = atoi(argv[1]);
    numCons = atoi(argv[2]);
    //Crear el arreglo de IDs para los productores conforme al número de productores.
	arrIdProds = (int *)malloc(sizeof(int) * numProds);
    arrIdCons = (int *)malloc(sizeof(int) * numCons);
    //Calcular el número total de consumos. 
	totalConsumos = numProds * PRODS;

    printf("[ %d %d %d]\n", numProds, numCons, totalConsumos);

    //Verificar datos validos.
    if(numProds < 1 || numCons < 1){
        printf("El número de productores/consumidores debe ser almenos 1.");
        return(-1);
    }

    key_t llave = ftok("/bin/ls", 1);

    if(llave < 0){
        printf("Error al crear llave.");
        return -1;
    }
    // [C, P, p1, c1, p2, c2]
    semId = semget(llave, 6,  0777 | IPC_CREAT | IPC_EXCL);
    if(semId < 0){
        printf("Error al crear el grupo de semaforos.");
        return -1;
    }

    //Paso 2: Inicializar semáforo.
    semctl(semId, 0, SETVAL, 2); // P
    semctl(semId, 1, SETVAL, 0); // C
   for(int i = 2; i < 6; i++){
       if(i%2 == 1){
           semctl(semId, i, SETVAL, 0);
       }else{
           semctl(semId, i, SETVAL, 1);
       }
   }

    //Paso 3: Crear de hilo productor e hilo consumidor.
    pthread_t *productores;
    pthread_t *consumidores;
    productores = (pthread_t *)malloc(numProds * sizeof(pthread_t));
    consumidores = (pthread_t *)malloc(numCons * sizeof(pthread_t));

    for(int i = 0; i < numProds; i++){
		arrIdProds[i] = i;
        pthread_create(&productores[i], NULL, escritura, (void *)&arrIdProds[i]);
    }
    for(int j=0; j < numCons; j++){
        arrIdCons[j] = j;
        pthread_create(&consumidores[j], NULL, lectura, (void *)&arrIdCons[j]);
    }

    for(int i=0; i < numProds; i++){
        pthread_join(productores[i], NULL);
    }

    for(int i=0; i < numCons; i++){
        pthread_join(consumidores[i],NULL);
    }

    eliminarSemaforo();
    return 0;

}