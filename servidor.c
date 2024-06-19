//// CONECTA 4 (servidor) ////

// PARA EJECUTARLO: ./servidor [puerto] [nº filas] [nº columnas]

// Ander Monreal Ayanz
// Santiago Gil Gil

/* ACLARACIONES DEL CODIGO

- JUGADOR 1 (0) --> X
- JUGADOR 2 (1) --> O

*/
#include <stdio.h>
#include <string.h>	//strlen
#include <sys/socket.h>	//socket
#include <arpa/inet.h>	//inet_addr
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <time.h>

#define LONGID 12
#define LONGNOM 18

/// DECLARACION DEL TIPO USUARIO ///
typedef struct {
    char *nombre;
    bool turno;
    bool conectado;
    bool ganador;
} Nombre;


/* Declaracion de variables */
Nombre *jugadores;
int *movimiento, *numeroDeClientes, *estado;
int shmNombres, shmMovimientos, shmConectados, shmEstado;
int  sock, c_sock, pid, filas, columnas;


/// CABECERAS DE FUNCIONES DECLARADAS ABAJO ///
void atenderConexion (int c_sock, FILE *f, char nombre[101]);
int prepararCliente (char *nombre, int id, int random);
int prepararJuego (FILE *f, int id, char tablero[filas][columnas]);
int turnamos(FILE*f, const int id, char tablero[filas][columnas]);
bool es_movimiento_posible (char tablero[filas][columnas], int col);
int realizar_movimiento (char tablero[filas][columnas], int col, int id);
int final_del_juego(char tablero[filas][columnas]);
void cambio_de_turno(int id);
int actualizar_tablero(char tablero[filas][columnas], int id);


//////// FUNCION PARA SALIR DEL PROGRAMA, MANDA SALIR TAMBIÉN A LOS DEMÁS  ////////
void onCtrlC () {
    close(sock);
    close(c_sock);

	/* Desvinculo de la zona de memoria */
	shmdt(jugadores);
	shmdt(movimiento);
	shmdt(numeroDeClientes);
	shmdt(estado);

	/* Eliminar las memorias compartidas */
	shmctl (shmNombres, IPC_RMID, NULL);
	shmctl (shmMovimientos, IPC_RMID, NULL);
	shmctl (shmConectados, IPC_RMID, NULL);
	shmctl (shmEstado, IPC_RMID, NULL);

	sleep(1);

	kill(-getpid(), SIGKILL); /* Mando señal de salida a los hijos */
	printf("\n\t[+] Programa finalizado!\n");
    exit(0);
}


//////// PARA CREAR EL ID ////////
void getid(char *s) {
    static const char alphanum[] = "abcdefghijklmnopqrstuvwxyz123456789";
    for (int i = 0; i < LONGID; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    s[LONGID] = 0;
}


//////// FUNCION PARA SABER SI EXISTE EL USUARIO O NO EXISTE ////////
bool existeUsuario(const char * const us, int c, char *  nom) {
	FILE* u;
	int x;
	bool b;
	char rid[LONGID+1];
	u = fopen("servidor.txt", "r");
	if(!feof(u)) {
		fscanf(u, "%s %d %s", rid, &x, nom);
		while(!feof(u) && (us != rid && c != x))
			fscanf(u, "%s %d %s", rid, &x, nom);
		b = ((strcmp(us, rid) == 0) && x == c);
	}
	else
		b = false;
	fclose(u);
	return b;
}




//////// FUNCION PARA CAMBIAR EL NOMBRE AL USUARIO ////////
void cambioNombre(const char* id, const char* nuevoNombre, bool *b) {
	char leid[LONGID+1], nombre[LONGNOM+1];
	int c, a;
    FILE* u, *tmp;
    u = fopen("servidor.txt","r");
    if(u == NULL) {
		perror("Error abriendo el archivo servidor");
		exit(-1);
	}
    tmp = fopen("tmp.txt", "w");
    if(tmp == NULL) {
		perror("Error abriendo el archivo archivo temporal\n");
		exit(-1);
	}
    *b = false;
    setbuf(u, NULL);
    while(fscanf(u, "%s %d %s", leid, &c, nombre) != EOF){
		setbuf(tmp, NULL);
		setbuf(u, NULL);
		if(strcmp(id, leid) == 0) {
			if((a = fprintf(tmp, "%s %d %s\n", leid, c, nuevoNombre)) < 0)
				perror("Error escribiendo el fichero de transito");
			*b = true;
		}
		else
			if((a = fprintf(tmp, "%s %d %s\n", leid, c, nombre)) < 0)
				perror("Error en la escritura del fichero\n");
	}
	fclose(u);
    fclose(tmp);
	if((a = remove("servidor.txt")) < 0)
		perror("No se ha podido borrar el archivo antiguo\n");
	if((a = rename("tmp.txt", "servidor.txt")) < 0)
		perror("No se ha podido modificar el nombre\n");
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

//////////////////////// MAIN ////////////////////////
int main (int argc, char *argv[]) {

	if (argc != 4) {
        printf("[+] Número invalido de argumentos!\n");
        exit(-1);
    }

    FILE *f, *fichero;
	int id;
	char nombre[101];

    filas = atoi(argv[2]); // Recojo el número de filas
    columnas = atoi(argv[3]); // Recojo el número de columnas
	char tablero[filas][columnas];

    struct sockaddr_in servidor, cliente; /* Para coger la familia de direcciones, IP y puerto */
    /* Rellenamos la estructura para pedir puerto */
    servidor.sin_family=AF_INET;
    servidor.sin_port=htons(atoi(argv[1]));
    servidor.sin_addr.s_addr=INADDR_ANY;

	signal(SIGINT, onCtrlC);
	sock = socket(PF_INET, SOCK_STREAM, 0); /* Abrimos el socket */


// INICIO CREACION DE ELEMENTO SNECESARIOS Y COMPROBACIONES DE ERROR

	// Creación de las claves
	key_t clave = ftok("/tmp", 20);
	if (clave == -1) {
		perror ("ERROR en la creación de la clave");
		exit(-1);
	}

// CREACION DE MEMORIAS COMPARTIDAS -----------------------------

	// Creacion de la memoria compartida de los nombres de los jugadores
	if ((shmNombres = shmget(clave, sizeof(Nombre)*2, 0600 | IPC_CREAT)) == -1) {
		perror("Error creando memoria compartida para los nombres de los jugadores");
		exit(-1);
	}

	// Creacion de la memoria compartida de los movimientos
	clave = ftok("/tmp", 22);
	if ((shmMovimientos = shmget(clave, sizeof(int), 0600 | IPC_CREAT)) == -1) {
		perror("Error creando memoria compartida para cada movimiento");
		exit(-1);
	}

	// Creacion de la memoria compartida del numero de clientes
	clave = ftok("/tmp", 23);
	if ((shmConectados = shmget(clave, sizeof(int), 0600 | IPC_CREAT)) == -1) {
		perror("Error creando memoria compartida el numero de clientes");
		exit(-1);
	}

	// Creacion de la memoria compartida del estado de la partida
	clave = ftok("/tmp", 24);
	if ((shmEstado = shmget(clave, sizeof(int), 0600 | IPC_CREAT)) == -1) {
		perror("Error creando memoria compartida del estado de la partida");
		exit(-1);
	}


// VINCULACIONES A MEMORIA COMPARTIDA -----------------------------

	// Vincuaación a la zona de memoria para los nombres de los jugadres
    if ((jugadores = (Nombre*) shmat(shmNombres, NULL, 0)) == (void*)-1) {
		perror("ERROR en la vinculación a la zona de memoria de los jugadores");
		exit(-1);
	}

	// Vincuaación a la zona de memoria para los movimientos
    if ((movimiento = (int*) shmat(shmMovimientos, NULL, 0)) == (void*)-1) {
		perror("ERROR en la vinculación a la zona de memoria de los movimientos");
		exit(-1);
	}

	// Vincuaación a la zona de memoria para el numero de clientes
    if ((numeroDeClientes = (int*) shmat(shmConectados, NULL, 0)) == (void*)-1) {
		perror("ERROR en la vinculación a la zona de memoria del numero de clientes");
		exit(-1);
	}

	// Vincuaación a la zona de memoria para el estado de la partida
    if ((estado = (int*) shmat(shmEstado, NULL, 0)) == (void*)-1) {
		perror("ERROR en la vinculación a la zona de memoria del estado de la partida");
		exit(-1);
	}


	/* Asigna direccion y puerto al socket sock */
    if (bind(sock, (struct sockaddr *)&servidor, sizeof(servidor)) == -1) {
        perror ("\nERROR, no se pudo coger el puerto correctamente\n");
        exit(-1);
    }

	printf("[+] Inicio del servidor. Escuchando conexiones en el puerto %d ....\n\n", atoi(argv[1]));
    listen(sock,5); /* Espera a una conexión, por tradición se pone el número 5 */

	printf("[+] Creación del tablero [%dx%d] \n", filas, columnas);

    for (int i = 0; i < filas; i++) {
		for (int j = 0; j < columnas; j++) {
            tablero[i][j] = '.';
        }
    }


    if(sock < 0)
			perror("[!] Conexion rechazada\n");

	*numeroDeClientes = 0;
	*estado = 3;
	int random = rand() % 2; // Generar número aleatorio entre 0 y 1
	while(1) {
		printf("<*> Numero de clientes conectados: (%d/2) \n", *numeroDeClientes);
        int dirlen;
        dirlen = sizeof(cliente);
		sleep(1);

		c_sock = accept(sock,(struct sockaddr *)&cliente, (socklen_t *)&dirlen); /* Aceptar la conexion con el cliente*/

        if (c_sock < 0) {
			perror("Error en conexion\n");
			continue;
		}


		if ((pid = fork()) == -1) {
			perror ("ERROR en la creacion del proceso hijo 1");
			exit(-1);
		}
		if (pid == 0) {
			*numeroDeClientes = *numeroDeClientes + 1;
			id = *numeroDeClientes - 1;
			f = fdopen (c_sock, "w+");
			if (*numeroDeClientes > 2) {
				printf("\n<*> Conexion abortada desde %s:%d, aforo completo (total = %d/2) \n", inet_ntoa(cliente.sin_addr), cliente.sin_port, id);
				setbuf (f, NULL);
				fprintf (f, "OCUPADO\n");
				*numeroDeClientes = *numeroDeClientes - 1;
				fclose(f);
				close (c_sock);
			}
			else {
				printf("\n<*> Nuevo clientes conectado (total = %d/2) desde %s:%d \n", id, inet_ntoa(cliente.sin_addr), cliente.sin_port);

				atenderConexion (c_sock, f, nombre);
				prepararCliente (nombre, id, random);
				if (prepararJuego (f, id, tablero) == -1) {
					perror ("\n\t[x] RROR en la función de preparar juego");
					exit(-1);
				}
				*numeroDeClientes = *numeroDeClientes - 1;
				fclose(f);
				close (c_sock);
				printf("<*> Cliente %s desconectado \n", nombre);
				sleep(1);
			}
		}
	}
}


//////////////////////////////// FUCNION PARA PREPARAR A LOS JUGADORES ////////////////////////////////
int prepararCliente (char *nombre, int id, int random) {
	jugadores[id].nombre = nombre;
	jugadores[id].conectado = true;
	jugadores[id].ganador = false;
	if (random == id)
		jugadores[id].turno = true;
	else
		jugadores[id].turno = false;

	return 0;
}


/////////////////////////////// FUNCION PARA PREPARAR EL JUEGO Y JUGAR ///////////////////////////////
int prepararJuego (FILE *f, int id, char tablero[filas][columnas]) {
	char buf[20000], str[20000];

	fprintf(f, "ID %d\n", id);

	if(!jugadores[0].conectado || !jugadores[1].conectado) {
        fprintf(f, "ESPERANDO\n");

        fgets(buf, sizeof(buf), f);
        sscanf(buf, "ESPERANDO %s", str);
        if(strcmp(str, "OK") != 0) {
            printf("Se ha producido un error\n");
            return -1;
        }

    }
	while(!jugadores[0].conectado || !jugadores[1].conectado) {
		;
    }

    setbuf(f, NULL);
    if(id == 0)
        fprintf(f, "START %s %d %d %d\n", jugadores[1].nombre, filas, columnas, id);
    else
        fprintf(f, "START %s %d %d %d\n", jugadores[0].nombre, filas, columnas, id);

	fgets(buf, sizeof(buf), f);
    sscanf(buf, "%s", str);
    if(strcmp(str, "ENCANTADO") == 0)
        turnamos(f, id, tablero);

    else {
        printf("Error a la hora de decir encantado\n");
        return -1;
    }
    return 0;
}

////////////////////////////// FUNCION PARA IR TURNANDO ENTRE JUGADORES //////////////////////////////
int turnamos(FILE*f, const int id, char tablero[filas][columnas]) {
    char buf[20000], str[20000];
    int col;
    char letra;
    bool b;


	// EMPIEZA HA FUNCIONAR Y TURNANDOSE
	if (jugadores[id].turno) {
		setbuf (f, NULL);

		fprintf (f, "URTURN %d\n", -5); // Le decimo que empieza

		fgets (buf, sizeof(buf), f);
		sscanf (buf, "COLUMN %d\n", &col);

		printf ("[+] Jugador: %d, mueve a --> %d\n", id, col);

		if (es_movimiento_posible (tablero, col)) {
			if (realizar_movimiento (tablero, col, id) == -1) {
				perror ("[x] Error en la funcion de realizar movimiento\n");
				return (-1);
			}
			printf ("[+] Movimiento válido por parte del jugador %d\n", id);
			fprintf (f, "COLUMN OK\n");
		}

		else {
			while (!es_movimiento_posible (tablero, col)) {
				printf ("[!] Movimiento no válido por parte del jugador %d\n", id);
				fprintf (f, "COLUMN ERROR\n");

				fgets (buf, sizeof(buf), f);
				sscanf (buf, "COLUMN %d\n", col); // Recibo la primera columna a mover
				printf ("[+] Jugador: %d, mueve a --> %d\n", id, col);
			}

			if (realizar_movimiento (tablero, col, id) == -1) {
				perror ("[x] Error en la funcion de realizar movimiento\n");
				return (-1);
			}
			fprintf (f, "COLUMN OK\n");
			printf ("[+] Movimiento válido por parte del jugador %d\n", id);
		}
		mostrar_tablero(tablero);
		cambio_de_turno(id);
	}

	while (*estado == 3) {
		while (jugadores[id].turno && *estado == 3) {
			setbuf (f, NULL);

			fprintf (f, "URTURN %d\n", *movimiento); // Su turno y el movimiento del rival
			actualizar_tablero(tablero, id);

			fgets (buf, sizeof(buf), f);
			sscanf (buf, "COLUMN %d\n", &col);

			printf ("[+] Jugador: %d, mueve a --> %d\n", id, col);

			if (es_movimiento_posible (tablero, col)) {
				if (realizar_movimiento (tablero, col, id) == -1) {
					perror ("[x] Error en la funcion de realizar movimiento\n");
					return (-1);
				}
				printf ("[+] Movimiento válido por parte del jugador %d\n", id);
				fprintf (f, "COLUMN OK\n");
			}

			else {
				while (!es_movimiento_posible (tablero, col)) {
					printf ("[!] Movimiento no válido por parte del jugador %d\n", id);
					fprintf (f, "COLUMN ERROR\n");

					fgets (buf, sizeof(buf), f);
					sscanf (buf, "COLUMN %d\n", col); // Recibo la primera columna a mover
					printf ("[+] Jugador: %d, mueve a --> %d\n", id, col);
				}

				if (realizar_movimiento (tablero, col, id) == -1) {
					perror ("[x] Error en la funcion de realizar movimiento\n");
					return (-1);
				}
				fprintf (f, "COLUMN OK\n");
				printf ("[+] Movimiento válido por parte del jugador %d\n", id);
			}
			*estado = final_del_juego(tablero);
			mostrar_tablero(tablero);
			cambio_de_turno(id);
		}
	}

	if (*estado == id) {
		jugadores[id].ganador = true;
		fprintf (f, "VICTORY\n");
		mostrar_tablero(tablero);
		printf("----------------------\nFin de la partida\n----------------------\n");
	}
	else if (*estado != id && *estado != 2) {
		fprintf (f, "DEFEAT %d\n", *movimiento);
		actualizar_tablero(tablero, id);
	}
	else if (*estado == 2)
		fprintf (f, "TIE\n");
	else {
		perror ("[X] ERROR se sale del bucle sin haber ganado nadie!\n");
		fprintf (f, "ERROR\n");
		exit(-1);
	}

    return 0;
}

int actualizar_tablero (char tablero[filas][columnas], int id) {
	int col = *movimiento;
	for (int i = filas; i > 0; i--){
		if (tablero[i][col] == '.') {
			if (id == 0) {
				tablero[i][col] = 'o';
				return 0;
			}
			else if (id == 1){
				tablero[i][col] = 'x';
				return 0;
			}
			else {
				perror ("[x] ERROR en el id de realiza_movimiento\n");
				return (-1);
			}
		}
	}
}


void cambio_de_turno (int id) {
	if (id == 0) {
		jugadores[0].turno = false;
		jugadores[1].turno = true;
	} else if (id == 1) {
		jugadores[0].turno = true;
		jugadores[1].turno = false;
	} else
		perror ("[X] ERROR en el cambio del turno \n");
}

bool es_movimiento_posible (char tablero[filas][columnas], int col) {
	if (col < 0 || col > columnas) {
		printf ("[!] Columna %d no existente\n", col);
		return false;
	}
	else {
		for (int i = 0; i < filas; i++) {
			if (tablero[i][col] == '.')
				return true;
		}
		printf ("[!] Columna %d llena\n", col);
		return false;
	}
}


int realizar_movimiento (char tablero[filas][columnas], int col, int id) {
	*movimiento = col;
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




// # PARA FINAL DEL JUEGO
// -1 error
// 0 gana juador 0
// 1 gana jugador 1
// 2 empate
// 3 todavia no ha ganado nadie

int final_del_juego (char tablero[filas][columnas]) {
	// Verifica si hay cuatro en raya en las filas
	for (int row = 0; row < filas; row++) {
		for (int col = 0; col < columnas - 3; col ++) {
			if (tablero[row][col] == tablero[row][col+1] && tablero[row][col+1] == tablero[row][col+2] && tablero[row][col+2] == tablero[row][col+3] && tablero[row][col] != '.') {
				if (tablero[row][col] == 'x') {
					tablero[row][col] = 'X';
					tablero[row][col+1] = 'X';
					tablero[row][col+2] = 'X';
					tablero[row][col+3] = 'X';
					return (0);
				}
				else if (tablero[row][col] == 'o') {
					tablero[row][col] = 'O';
					tablero[row][col+1] = 'O';
					tablero[row][col+2] = 'O';
					tablero[row][col+3] = 'O';
					return (1);
				}
				else {
					perror("[X] ERROR en la comprobacion de quien ha ganado\n");
					return (-1);
				}
			}

		}
	}

	// Verifica si hay cuatro en raya en las columnas
	for (int col = 0; col < columnas; col++) {
		for (int row = 0; row < filas - 3; row ++) {
			if (tablero[row][col] == tablero[row+1][col] && tablero[row+1][col] == tablero[row+2][col] && tablero[row+2][col] == tablero[row+3][col] && tablero[row][col] != '.') {
				if (tablero[row][col] == 'x') {
					tablero[row][col] = 'X';
					tablero[row+1][col] = 'X';
					tablero[row+2][col] = 'X';
					tablero[row+3][col] = 'X';
					return (0);
				}
				else if (tablero[row][col] == 'o') {
					tablero[row][col] = 'O';
					tablero[row+1][col] = 'O';
					tablero[row+2][col] = 'O';
					tablero[row+3][col] = 'O';
					return (1);
				}
				else {
					perror("[X] ERROR en la comprobacion de quien ha ganado\n");
					return (-1);
				}
			}
		}
	}

	// Verifica si hay cuatro en raya en las diagonales hacia la derecha
	for (int row = 0; row < filas - 3; row++) {
		for (int col = 0; col < columnas - 3; col++) {
			if (tablero[row][col] == tablero[row+1][col+1] && tablero[row+1][col+1] == tablero[row+2][col+2] && tablero[row+2][col+2] == tablero[row+3][col+3] && tablero[row][col] != '.') {
				if (tablero[row][col] == 'x') {
					tablero[row][col] = 'X';
					tablero[row+1][col+1] = 'X';
					tablero[row+2][col+2] = 'X';
					tablero[row+3][col+3] = 'X';
					return (0);
				}
				else if (tablero[row][col] == 'o') {
					tablero[row][col] = 'O';
					tablero[row+1][col+1] = 'O';
					tablero[row+2][col+2] = 'O';
					tablero[row+3][col+3] = 'O';
					return (1);
				}
				else {
					perror("[X] ERROR en la comprobacion de quien ha ganado\n");
					return (-1);
				}
			}
		}
	}

	// Verificar si hay cuatro en raya en las diagonales hacia la izquierda
	for (int row = 0; row < filas - 3; row++) {
		for (int col = 3; col < columnas; col++) {
			if (tablero[row][col] == tablero[row+1][col-1] && tablero[row+1][col-1] == tablero[row+2][col-2] && tablero[row+2][col-2] == tablero[row+3][col-3] && tablero[row][col] != '.') {
				if (tablero[row][col] == 'x') {
					tablero[row][col] = 'X';
					tablero[row+1][col-1] = 'X';
					tablero[row+2][col-2] = 'X';
					tablero[row+3][col-3] = 'X';
					return (0);
				}
				else if (tablero[row][col] == 'o') {
					tablero[row][col] = 'O';
					tablero[row+1][col-1] = 'O';
					tablero[row+2][col-2] = 'O';
					tablero[row+3][col-3] = 'O';
					return (1);
				}
				else {
					perror("[X] ERROR en la comprobacion de quien ha ganado\n");
					return (-1);
				}
			}
		}
	}

	// Verificar si quedan huecos en el tablero (si hay empate o no)
	for (int row = 0; row < filas; row++) {
		for (int col = 0; col < columnas; col++){
			if (tablero[row][col] == '.')
				return (3); // Si no ha ganado nadie y quedan huecos en el tablero
		}
	}
	return (2); // Si no hay ganador ni huecos en el tablero, resultará empate la partida
}


//////// FUNCION PARA ATENDER CONEXIONES ////////
void atenderConexion (int c_sock, FILE *f, char nombre[101]) {
	int x, y, c;
    char buf[20000], cha[20000], id[LONGID+1];
	bool b;
    FILE *g;

	setbuf(f, NULL);
	fgets(buf, sizeof(buf), f);
	sscanf(buf, "%s", cha);

    if(strcmp(cha, "REGISTRAR") == 0) {
		int a;
		printf("\t[+] Recibida peticion de registro\n");
		srand(time(NULL));
		x = rand() % 10;
		srand(time(NULL)+ getpid());
		y = rand() % 10;
		setbuf(f, NULL);

		printf("\t[+] Enviando operacion a resolver: %d + %d. \n", x, y);
		fprintf(f, "RESUELVE %d %d\n", x, y);
		fgets(buf, sizeof(buf), f);
		sscanf(buf, "%s %d", cha, &a);

		printf("\t[+] Esperando respuesta del cliente...\n\n");
		sleep(1);
		printf("\t[+] Respuesta obtenida: %d\n", a);
		if (a == x + y) {

			printf("\t[+] Correcto, procedemos a suministrarle id de usuario\n");
			getid(id);
			setbuf(f, NULL);

			g = fopen("servidor.txt", "a");
			setbuf(g, NULL);
			fprintf(g, "%s %d Invitado\n", id, x+y);
			strcpy(nombre, "Invitado");
			fclose(g);
			fprintf(f, "REGISTRADO OK %s\n", id);
			printf("\t[+] ID asignado: %s\n", id);

            // Parte de cambiar el nombre al usuario recien registrado
			fgets(buf, sizeof(buf), f);
            sscanf(buf, "SETNAME %s", nombre);
            cambioNombre(id, nombre, &b);
            printf("\t[+] Registrado %s con nombre %s completado con exito!!\n\n", id, nombre);
            if(b) {
                setbuf(f, NULL);
                fprintf(f, "SETNAME OK\n");
            }
            else {
                setbuf(f, NULL);
                printf("SETNAME ERROR\n");
            }
		}

		else {
			printf("\t[+] La solución NO es correcta, recibido %d en vez de %d, te quedas sin registrar\n", atoi(buf), x+y);
			setbuf(f, NULL);
			fprintf(f, "REGISTRADO ERROR\n");
			close(c_sock);
		}
	}
	else if(strcmp(cha, "LOGIN") == 0) {
		sscanf(buf, "LOGIN %s %d", id, &c);

		printf("\t[+] Recibida peticion de logeo del usuario: %s\n", id);
		if(!existeUsuario(id, c, nombre)) {
			setbuf(f, NULL);
			fprintf(f, "LOGIN ERROR\n");
			close(c_sock);
			printf("\t[+] Usuario no encontrado, expulsandolo ...\n");
		}
		else {
			setbuf(f, NULL);
			fprintf(f, "LOGIN OK %s\n", nombre);
			printf("\t[+] Usuario %s (%s) logeado con éxito!!\n\n", nombre, id);
		}
	}
}


/*   printf("[!] \n");

# Posibilidad de que cuando uno gane la partida o realize su ultimo movimiento, el otro se quede bloqueado en el bucle de while (!es_movimiento_posible (tablero, filas, columnas, col))
*/




