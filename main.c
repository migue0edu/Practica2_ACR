#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#define PRODS 1000
#define SECCS 5
#define NUMSEMS 12

typedef struct estr{
    int tipoDato;
    int dato;
} secrit;

secrit seccionCritica[SECCS];
int semId;
int totalConsumos;
int escriturasRestantes[4] = {PRODS, PRODS, PRODS, PRODS};

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
    FILE* log1 = fopen("./abecedarioMin.log", "a");
    FILE* log2 = fopen("./abecedarioMay.log", "a");
    FILE* log3 = fopen("./numeros.log", "a");
    FILE* log4 = fopen("./simbolos.log", "a");
    if(log1 == NULL || log2 == NULL || log3 == NULL || log4 == NULL ){
        printf("Error al abrir el archivo.");
        perror("Error en fopen:");
    }

    while(totalConsumos > 0){
        miWait(1);
        for(int i=0; i < SECCS; i++){
            //Preguntar por el estado del semáforo de la 1er pos. de la sección critica.
            if(semctl(semId, (i*2)+3, GETVAL) == 1) {
                miWait((i*2)+3); //Decrementar semáforo.
                totalConsumos--;
                if(seccionCritica[i].tipoDato == 1){//SI el tipo de dato es uno se imprime y guarda como entero.
                    printf("%d--> <%d> Leido de [%d]: %d \n",totalConsumos,*id, i, seccionCritica[i].dato);
                    if(escriturasRestantes[2] > 0){
                        fprintf(log3, "%d \n",seccionCritica[i].dato);
                        escriturasRestantes[2]--;
                    }
                } else{
                    printf("%d--> <%d> Leido de [%d]: %c \n",totalConsumos,*id, i, seccionCritica[i].dato);
                    if(seccionCritica[i].dato >= 97 && seccionCritica[i].dato <= 122 && escriturasRestantes[0] > 0){
                        fprintf(log1, "%c \n",seccionCritica[i].dato);
                        escriturasRestantes[0]--;
                    }
                    if(seccionCritica[i].dato >= 65 && seccionCritica[i].dato <= 90 && escriturasRestantes[1] > 0){
                        fprintf(log2, "%c \n",seccionCritica[i].dato);
                        escriturasRestantes[1]--;
                    }
                    if(seccionCritica[i].dato >= 33 && seccionCritica[i].dato <= 47 && escriturasRestantes[3] > 0){
                        fprintf(log4, "%c \n",seccionCritica[i].dato);
                        escriturasRestantes[3]--;
                    }
                }
                miSignal((i*2)+2);//Incrementa semáforo para escritura.
                miSignal(0);
            }
        }
    }

    fclose(log1);
    fclose(log2);
    fclose(log3);
    fclose(log4);
    printf("+++Exit+++");
    return NULL;


}
// semId: 	[ p , c, p1, c1, p2, c2, p3, c3, ..., p5, c5 ]  = 12
//			[ 4 , 0, 1,  0,  1,  0,  1,  0,  ..., 1,  0  ]

void *escritura(void *param){
    int *id = (int *)param;
    printf("-----------%d------------\n", *id);
    int datosEscritos = PRODS;
    int valProd;
    int valMax;
    int valMin;
    //Establecer los datos que creara segun el ID de cada hilo,, usando su valor decimal en ASCII.
    switch(*id){
        case 0: //Produce alfabeto minusculas.
            valProd = 97;
            valMin = 97;
            valMax = 122;
            break;
        case 1: //Produce alfabeto mayusculas.
            valProd = 65;
            valMin = 65;
            valMax = 90;
            break;
        case 2: //Produce numeros.
            valProd = 0;
            valMax = PRODS + 1;
            break;
        case 3://Produce simbolos
            valProd = 33;
            valMin = 33;
            valMax = 47;
    }


    while(datosEscritos > 0){
        miWait(0);
        //Preguntar por el estado del semáforo de la 1er pos. de la sección critica.
        for(int i=0; i < SECCS; i++){
            if(semctl(semId, (i*2)+2, GETVAL) == 1) {
                miWait((i*2)+2); //Decrementar semáforo.
                datosEscritos--;
                if(valProd > valMax)
                    valProd = valMin;

                seccionCritica[i].dato = valProd++;
                if(*id == 2){
                    seccionCritica[i].tipoDato = 1;//Establece que se leera un int.
                    printf("%d.- { %d } Escrito en [%d]: %d \n",PRODS - datosEscritos,*id, i, seccionCritica[i].dato);
                }else{
                    seccionCritica[i].tipoDato = 0; //Establece que se leera un char.
                    printf("%d.- { %d } Escrito en [%d]: %c \n",PRODS - datosEscritos,*id, i, seccionCritica[i].dato);
                }
                miSignal((i*2)+3);//Incrementa semáforo para lectura.
                miSignal(1);

            }
        }
    }
    printf("---Exit---");
    return NULL;
}


int main(int argc, char const *argv[]){
    int arrIdProds[4];
    int arrIdCons[3];

    totalConsumos = 4 * PRODS;

    printf("[%d]\n", totalConsumos);

    key_t llave = ftok("/bin/echo", 1);

    if(llave < 0){
        printf("Error al crear llave.");
        return -1;
    }
    // semId: 	[ p , c, p1, c1, p2, c2, p3, c3, ..., p5, c5 ]  = 12
    //			[ 4 , 0, 1,  0,  1,  0,  1,  0,  ..., 1,  0  ]
    semId = semget(llave, NUMSEMS,  0777 | IPC_CREAT | IPC_EXCL);
    if(semId < 0){
        printf("Error al crear el grupo de semaforos.");
        return -1;
    }

    //Paso 2: Inicializar semáforo.
    semctl(semId, 0, SETVAL, 1); // P
    semctl(semId, 1, SETVAL, 0); // C

    for(int i = 2; i < NUMSEMS; i++){
        if(i%2 == 1){
            semctl(semId, i, SETVAL, 0);
        }else{
            semctl(semId, i, SETVAL, 1);
        }
    }

    //Paso 3: Crear de hilo productor e hilo consumidor.
    pthread_t productores[4];
    pthread_t consumidores[3];

    for(int i = 0; i < 4; i++){
        arrIdProds[i] = i;
        pthread_create(&productores[i], NULL, escritura, (void *)&arrIdProds[i]);
    }
    for(int j=0; j < 3; j++){
        arrIdCons[j] = j;
        pthread_create(&consumidores[j], NULL, lectura, (void *)&arrIdCons[j]);
    }

    for(int i=0; i < 4; i++){
        pthread_join(productores[i], NULL);
    }

    for(int i=0; i < 3; i++){
        pthread_join(consumidores[i],NULL);
    }

    eliminarSemaforo();
    return 0;

}