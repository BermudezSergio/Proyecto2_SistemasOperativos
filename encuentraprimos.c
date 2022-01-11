/*

Sergio Bermudez Fernandez 

Proyecto 2 Sistemas Operativos

INSO 2ºC

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>

#define LONGITUD_MSG 100           // Payload del mensaje
#define LONGITUD_MSG_ERR 200       // Mensajes de error por pantalla

// Códigos de exit por error
#define ERR_ENTRADA_ERRONEA 2
#define ERR_SEND 3
#define ERR_RECV 4
#define ERR_FSAL 5

#define NOMBRE_FICH "primos.txt"
#define NOMBRE_FICH_CUENTA "cuentaprimos.txt"
#define CADA_CUANTOS_ESCRIBO 5

// rango de búsqueda, desde BASE a BASE+RANGO
#define BASE 800000000
#define RANGO 2000

// Intervalo del temporizador para RAIZ
#define INTERVALO_TIMER 5

// Códigos de mensaje para el campo mesg_type del tipo T_MESG_BUFFER
#define COD_ESTOY_AQUI 5           // Un calculador indica al SERVER que está preparado
#define COD_LIMITES 4              // Mensaje del SERVER al calculador indicando los límites de operación
#define COD_RESULTADOS 6           // Localizado un primo
#define COD_FIN 7                  // Final del procesamiento de un calculador

// Mensaje que se intercambia

typedef struct {
    long mesg_type;
    char mesg_text[LONGITUD_MSG];
} T_MESG_BUFFER;

int Comprobarsiesprimo(long int numero);
void Informar(char *texto, int verboso);
void Imprimirjerarquiaproc(int pidraiz,int pidservidor, int *pidhijos, int numhijos);
int ContarLineas();
static void alarmHandler(int signo);
int compruebaEntrada(char *cadena);
void limpiafichcont();


int cuentasegs;                   // Variable para el cómputo del tiempo total

int main(int argc, char* argv[])
{
	int i,j;
	long int numero;
	long int numprimrec;
	long int nbase;
	int nrango;
	int nfin;
	time_t tstart,tend;

	key_t key;
	int msgid;
	int pid, pidservidor, pidraiz, parentpid, mypid, pidcalc;
	int *pidhijos;
	int intervalo,inicuenta;
	int verbosity;
	T_MESG_BUFFER message;
	char info[LONGITUD_MSG_ERR];
	int numhijos;
	int contMSG=0;//Vatiable para contar el numero de mensage que se envia a Informar
	// Control de entrada, después del nombre del script debe figurar el número de hijos y el parámetro verbosity



	if(argc!=3){

	printf("Error en el numrero de parametros introducidos ejemplo:\n./encuentraprimos 1 1 \n\n");
	exit(ERR_ENTRADA_ERRONEA);

	}


	//Comprobamos que no haya letras ni carecteres no numericos en los parametros introducidos
	if(compruebaEntrada(argv[1]) && compruebaEntrada(argv[2])){

	//Convertimos en int  las cadenas numericas
		numhijos=atoi(argv[1]);
	verbosity=atoi(argv[2]);    

	//Tambien comprobamos que el numhijos no sea 0 y el verbosity solo puede tener el valor 0 y 1
	if(numhijos<=0){
		printf("El numero de procesos CALC no puede ser menor que 1\n\n");
		exit(ERR_ENTRADA_ERRONEA);
	}
	if(verbosity<0 || verbosity>1){
		printf("El verbosity solo puede tener parametros 0 y 1\n\n");
		exit(ERR_ENTRADA_ERRONEA);
	}

	}else{
	printf("Parametros de entrada con caracter no valido en alguna cadena %s o %s\n\n",argv[1],argv [2]);
	exit(ERR_ENTRADA_ERRONEA);
}

	limpiafichcont();

	//Guardamos el pid del padre asi ya lo conoce el hijo server
	pidraiz=getpid();


	pid=fork();       // Creación del SERVER
	pidservidor=pid;


	if (pid == 0)     // Rama del hijo de RAIZ (SERVER)
    	{
		pid = getpid();
		pidservidor=pid;	
		mypid = pidservidor;	   

		// Petición de clave para crear la cola
		if ( ( key = ftok( "/tmp", 'C' ) ) == -1 ) {
		  perror( "Fallo al pedir ftok" );
		  exit( 1 );
		}

		printf( "Server: System V IPC key = %u\n", key );

        	// Creación de la cola de mensajería
		if ( ( msgid = msgget( key, IPC_CREAT | 0666 ) ) == -1 ) {
		  perror( "Fallo al crear la cola de mensajes" );
		  exit( 2 );
		}
		printf("Server: Message queue id = %u\n", msgid );

        	i = 0;
        	// Creación de los procesos CALCuladores
		while(i < numhijos) {
		 if (pid > 0) { // Solo SERVER creará hijos
			 pid=fork(); 
			 if (pid == 0) 
			   {   // Rama hijo
				parentpid = getppid();
				mypid = getpid();
			   }
			 }
		 i++;  // Número de hijos creados
		}




                // AQUI VA LA LOGICA DE NEGOCIO DE CADA CALCulador.
		if (mypid != pidservidor)
		{
			//Enviamos el mensaje de que el hijo esta listo
			message.mesg_type = COD_ESTOY_AQUI;
			sprintf(message.mesg_text,"%d",mypid);
			msgsnd( msgid, &message, sizeof(message), IPC_NOWAIT);

			//Esperamos a que el SERV envia todas las bases y finales
			sleep(1);


			//Leomos la base y el fin de cada Calcl
                        msgrcv(msgid, &message, sizeof(message), 0, 0);
                        sscanf(message.mesg_text,"%ld %d",&nbase,&nfin);


			//Mientras no haya recorido su rango
  			for (numero=nbase;numero<nfin;numero++)
  			{
				//analiza si es primo o no
	 			if (Comprobarsiesprimo(numero)){
					//Si es primo envia el numero a SERV con el codigo de resultados		
					message.mesg_type = COD_RESULTADOS;
					sprintf(message.mesg_text,"%ld %d",numero,mypid);
					msgsnd( msgid, &message, sizeof(message), IPC_NOWAIT);


				}


  			}

			//Una vez acabado envia el mensaje al padre de que ya acabo y termina el hijo
			message.mesg_type = COD_FIN;
			sprintf(message.mesg_text,"%d",mypid);
			msgsnd( msgid, &message, sizeof(message), IPC_NOWAIT);
			
			//FIN CALC
			exit(0);
		}

		// SERVER

		else
		{

			tstart=time(NULL);

			// Pide memoria dinámica para crear la lista de pids de los hijos CALCuladores

 			pidhijos=(int*)malloc(numhijos*sizeof(int));

		  	//Recepción de los mensajes COD_ESTOY_AQUI de los hijos
		  	for (j=0; j <numhijos; j++)
		  	{
			  	msgrcv(msgid, &message, sizeof(message), 0, 0);
			  	sscanf(message.mesg_text,"%d",&pid); // Tendrás que guardar esa pid
				//Guardamos en una lista cada pid de los hijos Calc
    				pidhijos[j]=pid;
		  	}

			//Imprmimos la jerarquia una vez guardados todos los pid de los hijo CALC
			Imprimirjerarquiaproc(pidraiz,pidservidor,pidhijos,numhijos);

			//Calculamos el Rango y la base
			nrango=RANGO/numhijos;
			nbase=BASE;

			for(int indice=0;indice<numhijos;indice++){
				//Escribimos las bases y los rangos para cada hijo CALC con su COD_LIMITE
				nfin=nbase+nrango-1;
				
				//establezemos Cofigo
	            message.mesg_type = COD_LIMITES;
				
				//Enviamos el mensaje
        	    sprintf(message.mesg_text,"%ld %d",nbase,nfin);
                msgsnd( msgid, &message, sizeof(message), IPC_NOWAIT);
				//Incrementamos para el siguiente calcl
				nbase+=nrango;

			}

			//MIENTRAS HAYA HIJOS ANALIZAMOS EL EMSAJE SI ES COD FIN RESTAMOS UN HIJO SI ES COD NUM LO GUARDAMOS EN FICH Y LO ENVIAMOS A INFORMAR

			

			FILE *primos_fich=fopen(NOMBRE_FICH,"w");
			sleep(2);

			while(numhijos!=0){

				//Iniciamos el temporizador una vez se emepizan a procesar los datos;

	            msgrcv(msgid, &message, sizeof(message), 0, 0);
				
				//Analizamos el codigo del mensaje
				
				//Si es de tipo resultado
				if(message.mesg_type==COD_RESULTADOS){
					
					//leemos el mensaje y guardamos su valor de primo y pid del calc que lo encontroo
					sscanf(message.mesg_text,"%ld %d",&numprimrec ,&pidcalc); // Tendrás que guardar esa pid

					//En la vatiable texto vamos a guardar el mensaje que se le pasa a informar
				 	char texto[50];
					//Escribimos en el primos.txt el primo encontrado
					fprintf(primos_fich,"%ld\n",numprimrec);
					
					//Junamos en la vatiable texto el mensaje
		         	sprintf(texto,"MSG %d | %ld PID %d\n",contMSG,numprimrec,pidcalc);
					//Se lo enviamos y depende del verbosity se imprime o no
					
            	 	Informar(texto,verbosity);
					//Incrementamos el numero de PRIMO recibidos
			 		contMSG++;
			 	}
				else if(message.mesg_type==COD_FIN){
					//En caso de que haya cabado un hijo disminuimos el contador
					numhijos--;
					
					//Enviamos el mensaje del hijo calc finalizado
					sscanf(message.mesg_text,"%d",&pidcalc);
					//Avisamos de que hijo ha acabado y cuantos quedad
					printf("Fin de %d  Restantes: %d\n",pidcalc,numhijos);
				}


				sleep(0.2);
				//Cada vez que haya recibido 5 lo metemos en el fichero  cuentapirmos.txt
				if((contMSG%CADA_CUANTOS_ESCRIBO)==0){

					//Abrimos el fichero guardamos el contador y cerramos el fuchero
					FILE *cuentaprimos=fopen(NOMBRE_FICH_CUENTA,"w+");
					fprintf(cuentaprimos,"%d\n",contMSG); 
					fclose(cuentaprimos);

				}


			}//FIN WHILE


			//CERRAMOS EL FICHERO DONDE ESCRIBIAMOS LOS NUMEROS PRIMOS

			fclose(primos_fich);

			//REGISTRAMOS EL TIEMPO DE FINAL
			tend=time(NULL);
			
			//eL TIMEPO TOTAL EL EL TIEMPO QUE TARDO EN ACABAR MENOS EL QUE TARDO EN INICIAR
			printf("\n\nRANGO ANALIZADO\n\nTIEMPO TOTAL: %.2f\n",difftime(tend,tstart));

			//Guardamos el numero correcto en cuenta primos txt(el exacto)
            FILE *cuentaprimos=fopen(NOMBRE_FICH_CUENTA,"w+");
			fprintf(cuentaprimos,"%d\n",contMSG); 
			fclose(cuentaprimos);



			sleep(1);


			// Borrar la cola de mensajería, muy importante. No olvides cerrar los ficheros
			msgctl(msgid,IPC_RMID,NULL);

			exit(1);

	   	}
   	}
    // Rama de RAIZ, proceso primigenio
	else
	{
		
		//ENcviamos la señal de alarma y su controlador
		alarm(INTERVALO_TIMER);
		signal(SIGALRM, alarmHandler);

	
		//Mientras no haya acabado el servidor espera aqui;
		while(pidservidor!=wait(NULL)){
		}
	
		//LLamamos a la funcion para contar las lineas
		int contador=ContarLineas();
		printf("HAY %d PRIMOS\n\n\n",contador);

    }
}

void limpiafichcont(){

	//Creamos y limpiamos el fichero de cuentaprimos.txt
	FILE *creacontprimos=fopen(NOMBRE_FICH_CUENTA,"w+");
	int ini=0;
	fprintf(creacontprimos,"%d",ini);
	fprintf(creacontprimos,"\n");

}

// Manejador de la alarma en el RAIZ
static void alarmHandler(int signo)
{
		//Incrementamos el contador ya que la alarma suena cada 5 segundos
		cuentasegs+=5;
		int primosrec=0;
		
		//Leemos el multiplo de 5 guardado en cuentaprimos.txt
		FILE *readcuenta=fopen(NOMBRE_FICH_CUENTA,"r");

		//Si el fichero existe leemos la primera linea que es donde esta el numero de primos
		if(readcuenta!=NULL){

			//Lemos la linea
            fscanf(readcuenta,"%d",&primosrec);

			//Imprimimos los segundos y los primos registrados
            printf("%d Sec , Registrados: %d\n",cuentasegs,primosrec);
			fclose(readcuenta);

			//Iniciamos la alarma de nuevo
			alarm(INTERVALO_TIMER);
		}

}

int compruebaEntrada(char *cadena){

	//Suponemos que esta bien introducido
	int error=1;

	for(int posicion=0;posicion<strlen(cadena) && error;posicion++){
		if(cadena[posicion]<48||cadena[posicion]>57)
			error=0;//EN caso de que haya alguncaracte no numerico se produce el error
	}

	return error;
}


void Imprimirjerarquiaproc(int pidraiz,int pidservidor, int *pidhijos, int numhijos){

	//Imprimimos la jerarquia de los procesos
	printf("\n\nRAIZ\tSERV\tCALC\n");

	printf("%d\t%d\t%d\n",pidraiz,pidservidor,*pidhijos);

	//En caso de que haya mas de un proceso calc que los imprima bajo su titulo
	if(numhijos>1){
		for(int indice=1;indice<numhijos;indice++){
			printf("\t\t%d\n",(*pidhijos+indice));
		}
	}
}

int Comprobarsiesprimo(long int numero) {
  if (numero < 2) return 0; // Por convenio 0 y 1 no son primos ni compuestos
  else //Si no comprobamos hasta la mitad de su numero. EN caso de que no sea primo devolvemos 0
	for (int x = 2; x <= (numero / 2) ; x++)
		if (numero % x == 0) return 0;
  return 1;
}

void Informar(char *texto, int verboso){

	//En caso de que verboso sea 1 imprimomos el texto
	if(verboso==1){
		printf("%s",texto);
	}


}


int ContarLineas(){

	//funcion para contar las lienas de primos.txt asi sabemos cuantos primos hubo

	//Abrimos el fichero de primos.txt
	FILE *contprimos=fopen(NOMBRE_FICH,"r");

	//Si no es NULL es que existe
	if(contprimos!=NULL){

		int cont=0;
		long int primo=0;
		
		//leemos los primos y por cada uno incrementamos nuestro contador
		while((fscanf(contprimos,"%ld",&primo)!=EOF))
			cont++;

		//Cerramos el fichero
		fclose(contprimos);
		
		//Devolvemos el numero de lineas leidas
		return  cont;
		
	//En caso de que no exista terminamos el prtgrama
	}else{

		printf("Algo no funciono bien con ContarLineas");
		exit(1);
	}



}
