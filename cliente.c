//// CONECTA 4 (cliente) ////

// PARA EJECUTARLO: ./cliente [ip] [puerto] [nombre del jugador]

// Ander Monreal Ayanz
// Santiago Gil Gil

/* ACLARACIONES DEL CODIGO

- JUGADOR 1 (0) --> X
- JUGADOR 2 (1) --> O

*/
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>   //para usar shmat
#include <sys/sem.h>
#include <signal.h>

#define LONGNOM 18

// VARIABLES GLOBALES //
int shmTablero, sock, filas, columnas;

int realizar_movimiento (char tablero[filas][columnas], int col, int id);

//////// FUNCION PARA SALIR DEL PROGRAMA, MANDA SALIR TAMBIÉN A LOS DEMÁS  ////////
void onCtrlC () {
    printf("\n\t[+] Programa finalizado!\n");
    close(sock);
	sleep(1);
    exit(0);
}


/////////// FUNCION PARA MOSTRAR EL TABLERO //////////////
void mostrar_tablero (char tablero[filas][columnas]) {
    printf("\t\t\t\t");
    for (int i = 0; i < filas; i++) {
        for (int j = 0; j < columnas; j++)
            printf("%c ", tablero[i][j]);
        printf("\n\t\t\t\t");
    }
    for (int j = 0; j < columnas; j++)
        printf("%d ", j);
    printf("\n\n");
}


/* Funcion para imprimir el tablero recalcando el último movimiento del rival */
int mostrar_tablero_remarcado (int id, int col, char tablero[filas][columnas]) {
    int aux = filas;
    for (int i = filas; i > 0; i--) {
        if (tablero[i][col] == '.') {
            aux = i + 1;
            break;
        }
    }
    printf("\t\t\t\t");
    for (int i = 0; i < filas; i++) {
        for (int j = 0; j < columnas; j++) {
            if (i == aux && j == col) {
                if (tablero[i][col] == 'o')
                    printf("O ");
                else if (tablero[i][col] == 'x')
                    printf("X ");
                else {
                    printf("[+] Error en la funcion de mostrar_tablero_remarcado\n");
                    return(-1);
                }
            }
            else
                printf("%c ", tablero[i][j]);
        }
        printf("\n\t\t\t\t");
    }
    for (int j = 0; j < columnas; j++)
        printf("%d ", j);
    printf("\n\n");
}


//////////////// PROGRAMA PRINCIPAL ////////////////
int main(int argc, char* argv[]) {

    if (argc != 4 && argc != 3) {   // ./cliente[0], direccion IP [1], puerto del servidor[2], nombre del usuario[3]
        printf("[+] Número invalido de argumentos!\n");
        exit(-1);
    }
    
    FILE *f, *g;
    int x, y, c, col;
    char buf[20000], cha[20000], miNombre[101], nombreRival[101], identificador[101];
    int id = -1;
    
    signal(SIGINT, onCtrlC);
    struct sockaddr_in servidor;
    
    /* Rellenamos la estructura para pedir puerto */
    servidor.sin_family = AF_INET;
    servidor.sin_port = htons(atoi(argv[2]));
    servidor.sin_addr.s_addr = inet_addr(argv[1]);



    //creamos la misma clave que el servidor
    key_t clave = ftok("/tmp",20);
    if (clave == (key_t) -1)
        exit(-1);


    /* Abrimos el socket */
    if ((sock=socket(PF_INET,SOCK_STREAM,0)) == -1) {
        perror("Error de Socket ");
        exit(EXIT_FAILURE);
    }

    /* Conexion al servidor TCP */
    if (connect(sock , (struct sockaddr *) &servidor , sizeof(servidor)) < 0) {
		perror("Fallo en la conexion al servidor\n");
		exit(-1);
	} 

    /* Abrimos string sobre el socket */
    f = fdopen(sock, "r+");
    setbuf(f, NULL);


// PARTE DE REGISTRO O LOGIN DEL CLIENTE //
    if(access("cliente.txt", W_OK)) {

		printf("Nos registraremos en el servidor\n");
		fprintf(f, "REGISTRAR\n");
        fgets(buf, 20000, f);

        sscanf(buf,"%s %d %d", cha, &x, &y);
		printf("Recibidos enteros %d y %d\n", x, y);
        printf("Resultado de la suma %d + %d = %d\n", x, y, x+y);
		fprintf(f, "RESPUESTA %d\n", x+y);
        printf("Mandando respuesta %d\n", x+y);

        fgets(buf, 20000, f);
        sscanf(buf, "REGISTRADO %s %s", cha, identificador);

		if(strcmp(cha, "OK") == 0){

			printf("Sesion establecida con ID: %s\n", identificador);
			g = fopen("cliente.txt", "w");
            sscanf(buf,"REGISTRADO OK %s", identificador);
            setbuf(g, NULL);

            // printf("[+] Introduce tu nombre: ");
            // scanf("%100s", miNombre);
            strcpy(miNombre, argv[3]);

            fprintf(f, "SETNAME %s\n", miNombre);
            fgets (buf, sizeof(buf), f);
            sscanf(buf, "SETNAME %s", cha);

            if (strcmp(cha, "OK") != 0) {
                perror("[x] ERROR, Fallo en el setname\n");
                exit(-1);
            }
            fprintf(g, "%s %d\n", identificador, x+y);
			fclose(g);
            printf("[+] Bienvenido %s (%s), registrado exitoso!\n", miNombre, identificador);
		}
		else{
			printf("[!] Error en el registro\n");
			close(sock);
			exit(-1);
		}
	}
    else {
		g = fopen("cliente.txt", "r");
        fscanf(g, "%s %d", identificador, &c);
        printf("Hay datos para el usuario %s, probamos autentificacion\n", identificador);
        setbuf(f, NULL);
        fprintf(f, "LOGIN %s %d\n", identificador, c);
        fgets(buf, 20000, f);
        sscanf(buf, "LOGIN %s", cha);
        if(strcmp(cha,"ERROR") == 0){
            printf("[!] Error iniciando sesión, pruebe a borrar el archivo cliente.txt\n");
            close(sock);
            exit(0);
        }
        else if(strcmp(cha, "OK") == 0) {
            sscanf(buf, "LOGIN OK %s", miNombre);
            printf("[+] Bienvenido %s (%s), login exitoso!\n", miNombre, identificador);
        }
	}
// FIN DE LA PARTE DE REGISTRO O LOGIN //

    // Intetno de recibir el ID
    setbuf(f, NULL);
    fgets (buf, sizeof(buf), f);
    sscanf (buf, "ID %d", &id);

    if (id == 0) {
        // Intetno de recibir ESPERANDO
        fgets (buf, 20000, f);
        sscanf (buf, "%s", cha);

        if(strcmp(cha, "ESPERANDO") == 0) {

            fprintf(f, "ESPERANDO OK\n");
            printf("[+] Esperando a un contrincante...\n");
        } else {
            printf("ERROR esperando %s\n", buf);
            fprintf (f, "ESPERANDO ERROR\n");
            exit(-1);
        }
    }

    setbuf(f, NULL);
    fgets(buf, sizeof(buf), f);
    sscanf(buf, "%s", cha);

    if (strcmp(cha, "START") == 0) {
        sscanf (buf, "START %s %d %d %d", nombreRival, &filas, &columnas, &id);

        if (id == 0)
            printf("[+] Usted %s, sera el jugador %d, jugara con las X\n", miNombre, id);
        else if (id == 1)
            printf("[+] Usted %s, sera el jugador %d, jugara con los O\n", miNombre, id);
        else {
            printf("[x] ERROR, el ID recibido es %d\n", id);
            kill(getpid(), SIGINT);
        }
    }
    else {
        printf("ERROR start\n");
        fprintf (f, "START ERROR\n");
        exit (-1);
    }

    char tablero[filas][columnas];
    for (int i = 0; i < filas; i++) {
        for (int j = 0; j < columnas; j++) {
            tablero[i][j] = '.';
        }
    }

    printf("\n[+] Tenemos contrincante: %s\n", nombreRival);
    printf("[+] Dimensiones del tablero: %dx%d\n", filas, columnas);
    fprintf(f, "ENCANTADO\n");
    printf("[+] Inicio de la partida al 4 en raya\n\n");
    mostrar_tablero(tablero);


    setbuf(f, NULL);
    fgets(buf, sizeof(buf), f);
    sscanf(buf, "%s", cha);

     if (strcmp(cha, "URTURN") == 0) {
            sscanf (buf, "URTURN %d", &col);
            // printf("Columna: %d\n", col);
            if (col == -5) {
                printf("[+] Empiezas tu la partida, indica la columna en la que desee jugar: ");
                scanf("%d", &col);

                fprintf (f, "COLUMN %d\n", col);

                fgets (buf, sizeof(buf), f);
                sscanf(buf, "COLUMN %s", cha);
                while (strcmp(cha, "OK") != 0) {
                    printf("Columna insertada no valida, vuelve a introducirla: ");
                    scanf("%d", &col);
                    fprintf (f, "COLUMN %d\n", col);
                    printf("Esperando comprobacion por parte del servidor ...\n");
                    fgets (buf, sizeof(buf), f);
                    sscanf(buf, "COLUMN %s", cha);
                }
                realizar_movimiento (tablero, col, id);
                mostrar_tablero_remarcado(id, col, tablero);
            }
     }

    while (strcmp(cha, "DEFEAT") != 0 && strcmp(cha, "VICTORY") != 0 && strcmp(cha, "TIE")) {

        if (strcmp(cha, "URTURN") == 0) {
            sscanf (buf, "URTURN %d", &col);
            if (id == 0)
                realizar_movimiento (tablero, col, 1);
            else
                realizar_movimiento (tablero, col, 0);

            if (id == 0)
                printf("[+] Es tu turno (juegas con X), estado del tablero: \n\n");
            else
                printf("[+] Es tu turno (juegas con O), estado del tablero: \n\n");

            mostrar_tablero_remarcado(id, col, tablero);
            printf("[+] Indique la columna en la que desee jugar: ");
            scanf("%d", &col);

            fprintf (f, "COLUMN %d\n", col);
            printf("Esperando comprobacion por parte del servidor ...\n");

            fgets (buf, sizeof(buf), f);
            sscanf(buf, "COLUMN %s", cha);
            while (strcmp(cha, "OK") != 0) {
                printf("Columna insertada no valida, vuelve a introducirla: ");
                scanf("%d", &col);
                fprintf (f, "COLUMN %d\n", col);
                printf("\nEsperando comprobacion por parte del servidor ...\n");
                fgets (buf, sizeof(buf), f);
                sscanf(buf, "COLUMN %s", cha);
            }
            realizar_movimiento (tablero, col, id);
            mostrar_tablero_remarcado(id, col, tablero);
        }

        // Escaneo peticion del servidor //
        printf("[+] Turno del contrincante, esperando ... \n");
        setbuf(f, NULL);
        fgets(buf, sizeof(buf), f);
        sscanf(buf, "%s", cha);

    }

    printf ("[+] Fin de la partida al 4 en raya\n\n");
    mostrar_tablero(tablero);
    if (strcmp(cha, "VICTORY") == 0)
        printf("[+] ¡Enhorabuena %s has ganado ha %s! \n\n", miNombre, nombreRival);

    else if (strcmp(cha, "DEFEAT") == 0){
        printf("[+] Has peridio %s, %s te ha ganado \n\n", miNombre, nombreRival);
        sscanf(buf, "DEFEAT %d", &col);
        if (id == 0)
            realizar_movimiento (tablero, col, 1);
        else
            realizar_movimiento (tablero, col, 0);
    }

    else if (strcmp(cha, "TIE") == 0)
        printf("[+] Partida empatada\n\n");
    else
        printf("[x] No se que ha pasado, ha habido un error\n");


    printf("\n\t{+} Programa finalizado!\n");
    close(sock);
    fclose(f);
}

int realizar_movimiento (char tablero[filas][columnas], int col, int id) {
	for (int i = filas; i > 0; i--) {
		if (tablero[i][col] == '.') {
			if (id == 0) {
				tablero[i][col] = 'x';
				return 0;
			}
			else if (id == 1){
				tablero[i][col] = 'o';
				return 0;
			}
			else {
				perror ("[x] ERROR en el id de realiza_movimiento\n");
				return (-1);
			}
		}
	}
	printf ("[x] ERROR, no se ha encontrado hueco en la columna %d de la funcion realiza_movimiento\n", col);
	return (-1);
}



