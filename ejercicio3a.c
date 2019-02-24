#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#define PRODS 100
#define SECCS 2

struct secrit {
    char bandera;
    char cadena[10];
};

struct secrit seccionCritica[2];
int semId;

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
    int lecturas = PRODS * 2;

    while(lecturas > 0){
        miWait(1);
        for(int i=0; i < SECCS; i++){
            if(semctl(semId, i+2, GETVAL) == 1) {
                miWait(i+2); //Decrementar semáforo.
                if(seccionCritica[i].bandera == 0){
                    printf("%d>  Leido de [%d]: %s \n", PRODS * 2 - --lecturas, i, seccionCritica[i].cadena);
                    seccionCritica[i].bandera = 1;
                }
                miSignal(i+2);//Incrementa semáforo para lectura.
                break;
     	    }
        }
        miSignal(1);
	}
    //miSignal(1);

    return NULL;

}
//[ P, C, a1, a2 ]
//[ 2, 0, 1,  1  ]
void *escritura(void *param){
    int *id = (int *)param;
    int producciones = PRODS;
    printf("+++++++++++%d++++++++++++\n", *id);
     while(producciones > 0){
        miWait(0);
        for(int i=0; i < SECCS; i++){
            if(semctl(semId, i+2, GETVAL) == 1) {
                miWait(i+2); //Decrementar semáforo.
                if(seccionCritica[i].bandera == 1){
                    seccionCritica[i].bandera = 0;
                    if(*id == 0)
                        strcpy(seccionCritica[i].cadena, "ABC");
                    if(*id == 1)
                        strcpy(seccionCritica[i].cadena, "123");
                    printf("%d}  Escrito en [%d]: %s \n",PRODS - --producciones, i, seccionCritica[i].cadena);
                    //miSignal(1); 
                }
                miSignal(i+2);
                break;
            }
        }
        miSignal(0);
     }

    return NULL;
}


int main(int argc, char const *argv[]){
    int numProds;
    int numCons;
    int idProds[2];
    

    if(argc < 3){
        //printf("Uso: <Productores> <Consumidores>");
        //return -1;
        numProds = 2;
        numCons = 1;
    }else{
        numProds = atoi(argv[1]);
        numCons = atoi(argv[2]);
    }



    printf("[ %d %d ]\n", numProds, numCons);

    if(numProds < 1 || numCons < 1){
        printf("El número de productores/consumidores debe ser almenos 1.");
        return(-1);
    }

    key_t llave = ftok("/bin/ls", 1);

    if(llave < 0){
        printf("Error al crear llave.");
        return -1;
    }
    // [C, P, f1, f1]
    semId = semget(llave, 4,  0777 | IPC_CREAT | IPC_EXCL);
    if(semId < 0){
        printf("Error al crear el grupo de semaforos.");
        return -1;
    }

    seccionCritica[1].bandera = 1;
    seccionCritica[2].bandera = 1;

    //Paso 2: Inicializar semáforo.

    semctl(semId, 0, SETVAL, 2); // P
    semctl(semId, 1, SETVAL, 1); // C
    semctl(semId, 2, SETVAL, 1);
    semctl(semId, 3, SETVAL, 1);


    //Paso 3: Crear de hilo productor e hilo consumidor.
    pthread_t productores[2];
    pthread_t consumidores;


    for(int i = 0; i < 2; i++){
        idProds[i] = i;
        pthread_create(&productores[i], NULL, escritura, &idProds[i]);
    }
   
    pthread_create(&consumidores, NULL, lectura, NULL);
    

    for(int i=0; i < 2; i++){
        pthread_join(productores[i], NULL);
    }

    pthread_join(consumidores,NULL);
    
    eliminarSemaforo();
    return 0;

}