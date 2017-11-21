/*
 * Worker.h

 *
 *  Created on: 18/9/2017
 *      Author: Dario Poma
 */

#include "../../Biblioteca/src/Biblioteca.c"

//--------------------------------------- Constantes -------------------------------------

#define FRACASO -800
#define EXITO 1
#define DESCONEXION 0
#define TRANSFORMACION 303
#define REDUCCION_LOCAL 304
#define REDUCCION_GLOBAL 305
#define ALMACENADO_FINAL 306
#define ENVIAR_TEMPORAL 307
#define ENVIAR_LINEA 308
#define PEDIR_LINEA 309
#define CONEXION_WORKER 310

#define INTSIZE sizeof(Entero)
#define TEMPSIZE 12
#define DIRSIZE 40

#define RUTA_CONFIG "/home/utnso/Escritorio/tp-2017-2c-El-legado-del-Esqui/Worker/NodoConfig.conf"
#define RUTA_LOG "/home/utnso/Escritorio/tp-2017-2c-El-legado-del-Esqui/Worker/WorkerLog.log"
#define RUTA_TEMP "/home/utnso/Escritorio/temp/"

//--------------------------------------- Estructuras -------------------------------------

typedef struct {
	char ipFileSystem[20];
	char puertoFileSystemDataNode[20];
	char puertoFileSystemWorker[20];
	char puertoMaster[20];
	char nombreNodo[10];
	char rutaDataBin[255];
	char ipPropia[20];
} Configuracion;

typedef struct {
	int scriptSize;
	String script;
	int numeroBloque;
	int bytesUtilizados;
	char nombreResultado[12];
} Transformacion;

typedef struct {
	int scriptSize;
	String script;
	int cantidadTemporales;
	String nombresTemporales;
	char nombreResultado[12];
} ReduccionLocal;

typedef struct {
	Dir nodo;
	char temporal[TEMPSIZE];
} Nodo;

typedef struct {
	String linea;
	Socket socketWorker;
} Apareo;

typedef struct {
	Nodo* nodos;
	int scriptSize;
	String script;
	int cantidadNodos;
	String pathApareo;
	char nombreResultado[12];
} ReduccionGlobal;

typedef void* BloqueWorker;

//--------------------------------------- Globales -------------------------------------

Configuracion* configuracion;
ArchivoLog archivoLog;
String campos[7];
Socket listenerWorker;
Socket listenerMaster;
File dataBin;
Puntero punteroDataBin;
int estadoWorker;
int dataBinBloques;
int dataBinTamanio;

//--------------------------------------- Funciones de Worker -------------------------------------

void workerIniciar();
void workerAtenderMasters();
void masterDesconectar();
void workerAceptarMaster();
void masterRealizarOperacion(Socket unSocket);
void workerAtenderProcesos();
void workerAtenderWorkers();
void workerAceptarWorker();
void reduccionGlobalEnviarLinea(Mensaje* mensaje, Socket socketWorker);

//--------------------------------------- Funciones de Configuracion  -------------------------------------

void configuracionImprimir(Configuracion* configuracion);
void configuracionIniciarLog();
void configuracionIniciarCampos();
void configuracionCalcularBloques();
void configuracionSenial(int senial);
void configuracionIniciar();

//--------------------------------------- Funciones de Master -------------------------------------

void workerAceptarMaster();
void masterAtenderOperacion(Socket unSocket);
void masterRealizarOperacion(Socket unSocket);

//--------------------------------------- Funciones de Transformacion -------------------------------------

int transformacionEjecutar(Transformacion* transformacion);
void transformacion(Mensaje* mensaje, Socket unSocket);
void transformacionDestruir(Transformacion* transformacion);
void transformacionExito(Entero numeroBloque, Socket unSocket);
void transformacionFracaso(Entero numeroBloque, Socket unSocket);
void transformacionObtenerScript(Transformacion* transformacion, Mensaje* mensaje);
void transformacionFinalizar(Socket unSocket, int* estado);
void transformacionProcesarBloque(Transformacion* transformacion, Mensaje* mensaje, Socket unSocket);
String transformacionCrearBloque(Transformacion* transformacion);
String transformacionCrearScript(Transformacion* transformacion);


//--------------------------------------- Funciones de Reduccion Local -------------------------------------

void reduccionLocal(Mensaje* mensaje, Socket unSocket);
ReduccionLocal* reduccionLocalRecibirDatos(Puntero datos);
int reduccionLocalEjecutar(ReduccionLocal* reduccion, String temporales);
String reduccionLocalCrearScript(ReduccionLocal* reduccion);
String reduccionLocalObtenerTemporales(ReduccionLocal* reduccion);
void reduccionLocalDestruir(ReduccionLocal* reduccion);
void reduccionLocalFinalizar(int resultado, Socket unSocket);
void reduccionLocalExito(Socket unSocket);
void reduccionLocalFracaso(Socket unSocket);

//--------------------------------------- Funciones de Reduccion Global -------------------------------------

void reduccionGlobal(Mensaje* mensaje, Socket unSocket);
ReduccionGlobal* reduccionGlobalRecibirDatos(Puntero datos);
int reduccionGlobalEjecutar(ReduccionGlobal* reduccion);
void reduccionGlobalAparearTemporales(ReduccionGlobal* reduccion);
void reduccionGlobalAlgoritmoApareo(ReduccionGlobal* reduccion, Lista listaApareados);
String reduccionGlobalCrearScript(ReduccionGlobal* reduccion);
void reduccionGlobalRealizarConexiones(ReduccionGlobal* reduccion, Lista listaApareados);
void reduccionGlobalDestruir(ReduccionGlobal* reduccion);
void reduccionGlobalFinalizar(int resultado, Socket unSocket);
void reduccionGlobalExito(Socket unSocket);
void reduccionGlobalFracaso(Socket unSocket);
String reduccionGlobalEncargadoPedirLinea(Socket unSocket);
Apareo* reduccionGlobalLineaMasCorta(Apareo* unApareo, Apareo* otroApareo);
void reduccionGlobalControlarLineas(Lista listaApareados);

//--------------------------------------- Funciones de Almacenado Final -------------------------------------

void almacenadoFinal(Mensaje* mensaje, Socket socketMaster);
int almacenadoFinalEnviar(Puntero buffer, int tamanio, String pathYama);
void almacenadoFinalFinalizar(int resultado, Socket unSocket);
void almacenadoFinalFracaso(Socket unSocket);
void almacenadoFinalExito(Socket unSocket);

//--------------------------------------- Funciones de DataBin -------------------------------------

void dataBinAbrir();
void configuracionCalcularBloques();
Puntero dataBinMapear();
void dataBinConfigurar();
BloqueWorker bloqueBuscar(Entero numeroBloque);
BloqueWorker getBloque(Entero numeroBloque);
