/*
 ============================================================================
 Name        : YAMA.c
 Author      : Dario Poma
 Version     : 1.0
 Copyright   : Todos los derechos reservados papu
 Description : Proceso YAMA
 ============================================================================
 */

#include "YAMA.h"


int main(void) {
	yamaIniciar();
	yamaAtender();
	imprimirMensaje(archivoLog, "[EJECUCION] Proceso YAMA finalizado");
	return EXIT_SUCCESS;
}

void yamaIniciar() {
	pantallaLimpiar();
	estadoYama=ACTIVADO;
	imprimirMensajeProceso("# PROCESO YAMA");
	archivoLog=archivoLogCrear(RUTA_LOG, "YAMA");
	configuracion=malloc(sizeof(Configuracion));
	configurar();
	void sighandler(){ //como no maneja variables locales no importa que se vaya de scope
		configuracion->reconfigurar=true;
	}
	signal(SIGUSR1,sighandler);

	servidor = malloc(sizeof(Servidor));
	imprimirMensajeDos(archivoLog, "[CONEXION] Realizando conexion con File System (IP: %s | Puerto %s)", configuracion->ipFileSystem, configuracion->puertoFileSystem);
	servidor->fileSystem = socketCrearCliente(configuracion->ipFileSystem, configuracion->puertoFileSystem, ID_YAMA);
	imprimirMensaje(archivoLog, "[CONEXION] Conexion exitosa con File System");

	//infoNodos es una lista de ips y puertos
	Mensaje* infoNodos=mensajeRecibir(servidor->fileSystem);
	mensajeObtenerDatos(infoNodos,servidor->fileSystem);
	int i=0;
	while(i<infoNodos->header.tamanio/IPPORTSIZE){
		Worker worker;
		worker.conectado=true;
		worker.carga=0;
		worker.tareasRealizadas=0;
		worker.nodo=i; //pensando en borrar esto y manejarme solo con ip
		memcpy(worker.ipYPuerto,infoNodos->datos+IPPORTSIZE*i,IPPORTSIZE);
		list_add(workers,&worker);
	}
}

void configurar(){
	char* campos[7] = {"IP_PROPIO","PUERTO_MASTER","IP_FILESYSTEM","PUERTO_FILESYSTEM","RETARDO_PLANIFICACION","ALGORITMO_BALANCEO","DISPONIBILIDAD_BASE"};
	ArchivoConfig archivoConfig = archivoConfigCrear(RUTA_CONFIG, campos);
	stringCopiar(configuracion->puertoMaster, archivoConfigStringDe(archivoConfig, "PUERTO_MASTER"));
	stringCopiar(configuracion->ipFileSystem, archivoConfigStringDe(archivoConfig, "IP_FILESYSTEM"));
	stringCopiar(configuracion->puertoFileSystem, archivoConfigStringDe(archivoConfig, "PUERTO_FILESYSTEM"));
	configuracion->retardoPlanificacion = archivoConfigEnteroDe(archivoConfig, "RETARDO_PLANIFICACION");
	stringCopiar(configuracion->algoritmoBalanceo, archivoConfigStringDe(archivoConfig, "ALGORITMO_BALANCEO"));
	configuracion->disponibilidadBase = archivoConfigEnteroDe(archivoConfig, "DISPONIBILIDAD_BASE");
	configuracion->reconfigurar=false;
	archivoConfigDestruir(archivoConfig);
}

void yamaAtender() {
	servidor->maximoSocket = 0;
	listaSocketsLimpiar(&servidor->listaMaster);
	listaSocketsLimpiar(&servidor->listaSelect);

	void servidorControlarMaximoSocket(Socket unSocket) {
		if(unSocket>servidor->maximoSocket)
			servidor->maximoSocket = unSocket;
	}
	imprimirMensajeUno(archivoLog, "[CONEXION] Esperando conexiones de un Master (Puerto %s)", configuracion->puertoMaster);
	servidor->listenerMaster = socketCrearListener(configuracion->puertoMaster);
	listaSocketsAgregar(servidor->listenerMaster, &servidor->listaMaster);
	imprimirMensaje(archivoLog, "[CONEXION] Conexion exitosa con Master");
	servidorControlarMaximoSocket(servidor->fileSystem);
	servidorControlarMaximoSocket(servidor->listenerMaster);

	tablaEstados=list_create();
	tablaUsados=list_create();

	while(estadoYama==ACTIVADO){
		if(configuracion->reconfigurar)
			configurar();

		dibujarTablaEstados();

		servidor->listaSelect = servidor->listaMaster;
		socketSelect(servidor->maximoSocket, &servidor->listaSelect);
		Socket socketI;
		Socket maximoSocket = servidor->maximoSocket;
		for(socketI = 0; socketI <= maximoSocket; socketI++){
			if (listaSocketsContiene(socketI, &servidor->listaSelect)){ //se recibio algo
				//podría disparar el thread aca o antes de planificar
				if(socketI==servidor->listenerMaster){
					Socket nuevoSocket;
					nuevoSocket = socketAceptar(socketI, ID_MASTER);
					if(nuevoSocket != ERROR) {
						log_info(archivoLog, "[CONEXION] Proceso Master %d conectado exitosamente",nuevoSocket);
						listaSocketsAgregar(nuevoSocket, &servidor->listaMaster);
						servidorControlarMaximoSocket(nuevoSocket);
					}
				}else if(socketI==servidor->fileSystem){
					Mensaje* mensaje = mensajeRecibir(socketI);
					mensajeObtenerDatos(mensaje,socketI);
					if(mensaje->header.operacion==DESCONEXION){
						int nodoDesconectado=*((int32_t*)mensaje->datos);
						bool nodoDesconectadoF(Worker* worker){
							return worker->nodo==nodoDesconectado;
						}
						((Worker*)list_find(workers,nodoDesconectadoF))->conectado=false;

						void cazarEntradasDesconectadas(Entrada* entrada){
							if(entrada->nodo==nodoDesconectado){
								actualizarTablaEstados(entrada,Abortado);
							}
						}
						list_iterate(tablaEstados,cazarEntradasDesconectadas);
						//podría haber un mensaje de reconexion
						//si es que los nodos pueden reconectarse
					}else{
						Socket masterid;
						memcpy(&masterid,mensaje->datos,INTSIZE);
						log_info(archivoLog, "[RECEPCION] lista de bloques para master #%d recibida",&masterid);
						if(listaSocketsContiene(masterid,&servidor->listaMaster)) //por si el master se desconecto
							yamaPlanificar(masterid,mensaje+INTSIZE,mensaje->header.tamanio-INTSIZE);
					}
					mensajeDestruir(mensaje);
				}else{ //master
					Mensaje* mensaje = mensajeRecibir(socketI);
					mensajeObtenerDatos(mensaje,socketI);
					if(mensaje->header.operacion==Solicitud){
						int32_t masterid = socketI;
						//el mensaje es el path del archivo
						//aca le acoplo el numero de master y se lo mando al fileSystem
						mensaje=realloc(mensaje->datos,mensaje->header.tamanio+INTSIZE);
						memmove(mensaje->datos+INTSIZE,mensaje,mensaje->header.tamanio);
						memcpy(mensaje->datos,&masterid,INTSIZE);
						mensajeEnviar(servidor->fileSystem,Solicitud,mensaje->datos,mensaje->header.tamanio+INTSIZE);
						imprimirMensajeUno(archivoLog, "[ENVIO] path de master #%d enviado al fileSystem",&socketI);
					}else if(mensaje->header.operacion==DESCONEXION){
						listaSocketsEliminar(socketI, &servidor->listaMaster);
						socketCerrar(socketI);
						if(socketI==servidor->maximoSocket)
							servidor->maximoSocket--; //no debería romper nada
						log_info(archivoLog, "[CONEXION] Proceso Master %d se ha desconectado",socketI);
					}else{
						int32_t nodo=*((int32_t*)mensaje->datos);
						int32_t bloque=*((int32_t*)(mensaje->datos+INTSIZE));
						bool buscarEntrada(Entrada* entrada){
							return entrada->nodo==nodo&&entrada->bloque==bloque;
						}
						actualizarTablaEstados(list_find(tablaEstados,buscarEntrada),mensaje->header.operacion);
					}
					mensajeDestruir(mensaje);
				}
			}
		}
	}
}

void yamaPlanificar(Socket master, void* listaBloques,int tamanio){
	int i=0;
	Lista bloques=list_create();
	Lista byteses=list_create();
	while((sizeof(Bloque)*2+INTSIZE)*i<=tamanio){
		list_add(bloques,(Bloque*)(listaBloques+(sizeof(Bloque)*2+INTSIZE)*i));
		list_add(bloques,(Bloque*)(listaBloques+(sizeof(Bloque)*2+INTSIZE)*i+sizeof(Bloque)));
		list_add(byteses,(int32_t*)(listaBloques+(sizeof(Bloque)*2+INTSIZE)*i+sizeof(Bloque)*2));
		i++;
	}
	Lista tablaEstadosJob;
	job++;//mutex (supongo que las variables globales se comparten entre hilos)
	for(i=0;i<bloques->elements_count/2;i++){
		Entrada entrada;
		entrada.job=job;
		entrada.masterid=master;
		darPathTemporal(&entrada.pathTemporal,'t');
		list_add(tablaEstadosJob,&entrada);
	}

	if(stringIguales(configuracion->algoritmoBalanceo,"Clock")){
		void setearDisponibilidad(Worker* worker){
			worker->disponibilidad=configuracion->disponibilidadBase;
		}
		list_iterate(workers,setearDisponibilidad);
	}else if(stringIguales(configuracion->algoritmoBalanceo,"W-Clock")){
		int cargaMaxima=0;
		void obtenerCargaMaxima(Worker* worker){
			if(worker->carga>cargaMaxima)
				cargaMaxima=worker->carga;
		}
		void setearDisponibilidad(Worker* worker){
			worker->disponibilidad=configuracion->disponibilidadBase
					+cargaMaxima-worker->carga;
		}
		list_iterate(workers,obtenerCargaMaxima);
		list_iterate(workers,setearDisponibilidad);
	}else{
		imprimirMensaje(archivoLog,"[] no se reconoce el algoritmo");
		abort();
	}

	int clock=0;
	{
		int mayorDisponibilidad=0;
		void setearClock(Worker* worker){
			if(worker->disponibilidad>mayorDisponibilidad){
				mayorDisponibilidad=worker->disponibilidad;
				clock=worker->nodo;
			}
		}
		list_iterate(workers,setearClock);
	}
	Worker* obtenerWorker(int* pos){
		Worker* worker=list_get(workers,*pos);
		if(worker->conectado)
			return worker;
		*pos=*pos+1%workers->elements_count;
		return obtenerWorker(pos);
		//bucle infinito si todos los workers se desconectan.
		//Supongo que el control de eso debería estar en otro lado
	}
	Worker* workerClock=obtenerWorker(&clock);
	for(i=0;i<bloques->elements_count;i+=2){
		Bloque* bloque0 = list_get(bloques,i);
		Bloque* bloque1 = list_get(bloques,i+1);
		int* bytes=list_get(byteses,i/2);
		void asignarBloque(Worker* worker,Bloque* bloque,Bloque* alt){
			worker->carga++; //y habría que usar mutex aca
			worker->disponibilidad--;
			worker->tareasRealizadas++;
			Entrada* entrada=list_get(tablaEstadosJob,i/2);
			entrada->nodo=worker->nodo;
			entrada->bloque=bloque->bloque;
			entrada->bytes=*bytes;
			entrada->nodoAlt=alt->nodo;
			entrada->bloqueAlt=alt->bloque;
			entrada->etapa=Transformacion;
			entrada->estado=EnProceso;
		}

		bool encontrado=false;
		if(workerClock->nodo==bloque0->nodo){
			asignarBloque(workerClock,bloque0,bloque1);
			encontrado=true;
		}else if(workerClock->nodo==bloque1->nodo){
			asignarBloque(workerClock,bloque1,bloque0);
			encontrado=true;
		}
		if(encontrado){
			clock=(clock+1)%workers->elements_count;
			Worker* workerTest=obtenerWorker(&clock);
			if(workerTest->disponibilidad==0)
				workerTest->disponibilidad=configuracion->disponibilidadBase;
			continue;
		}
		int clockAdv=clock;
		while(1){
			clockAdv=(clockAdv+1)%workers->elements_count;
			if(clockAdv==clock){
				void sumarDisponibilidadBase(Worker* worker){
					worker->disponibilidad+=configuracion->disponibilidadBase;
				}
				list_iterate(workers,sumarDisponibilidadBase);
			}
			Worker* workerAdv=obtenerWorker(&clockAdv);
			if(workerAdv->disponibilidad>0){
				if(workerAdv->nodo==bloque0->nodo){
					asignarBloque(workerAdv,bloque0,bloque1);
					break;
				}else if(workerClock->nodo==bloque1->nodo){
					asignarBloque(workerAdv,bloque1,bloque0);
					break;
				}
			}
		}
	}

	int tamanioEslabon=INTSIZE*2+TEMPSIZE;
	int32_t tamanioDato=tamanioEslabon*tablaEstadosJob->elements_count;
	void* dato=malloc(tamanioDato);
	for(i=0;i<tablaEstadosJob->elements_count;i++){
		Entrada* entrada=list_get(tablaEstadosJob,i);
		memcpy(dato+tamanioEslabon*i,&entrada->nodo,INTSIZE);
		memcpy(dato+tamanioEslabon*i+INTSIZE,&entrada->bloque,INTSIZE);
		memcpy(dato+tamanioEslabon*i+INTSIZE*2,entrada->pathTemporal,TEMPSIZE);
	}
	mensajeEnviar(master,Transformacion,dato,tamanioDato);
	free(dato);

	list_add_all(tablaEstados,tablaEstadosJob); //mutex
	list_destroy(tablaEstadosJob);
}

void actualizarTablaEstados(Entrada* entradaA,Estado actualizando){
	void moverAUsados(bool(*cond)(void*)){
		//mutex
		Entrada* entrada;
		while((entrada=list_remove_by_condition(tablaEstados,cond))){
			list_add(tablaUsados,entrada);
		}
	}
	void darDatosEntrada(Entrada* entrada){
		entrada->nodo=entradaA->nodo;
		entrada->job=entradaA->job;
		entrada->masterid=entradaA->masterid;
		entrada->estado=EnProceso;
		entrada->bloque=-1;
	}
	entradaA->estado=actualizando;
	if(actualizando==Error||actualizando==Abortado){
		void abortarJob(){
			bool abortarEntrada(Entrada* entrada){
				if(entrada->job==entradaA->job){
					entrada->estado=Abortado;
					return true;
				}
				return false;
			}
			moverAUsados(abortarJob);
			mensajeEnviar(entradaA->masterid,Aborto,nullptr,0);//podría ser cierre, depende como lo implemente
		}
		if(entradaA->etapa==Transformacion&&actualizando!=Abortado){
			if(entradaA->nodo==entradaA->nodoAlt){
				abortarJob();
				return;
			}
			Entrada alternativa;
			darDatosEntrada(&alternativa);
			alternativa.nodo=entradaA->nodoAlt;
			alternativa.bloque=entradaA->bloqueAlt;
			list_add(tablaEstados,&alternativa);
			bool buscarError(Entrada* entrada){
				return entrada->estado==Error;
			}
			list_add(tablaUsados,list_remove_by_condition(tablaEstados,buscarError));//mutex
		}else{
			abortarJob();
		}
		return;
	}
	bool trabajoTerminadoB=true;
	void trabajoTerminado(bool(*cond)(void*)){
		void aux(Entrada* entrada){
			if(cond(entrada)&&entrada->estado!=Terminado)
				trabajoTerminadoB=false;
		}
		list_iterate(tablaEstados,aux);
	}
	bool mismoJob(Entrada* entrada){
		return entrada->job==entradaA->job;
	}
	bool mismoNodo(Entrada* entrada){
		return mismoJob(entrada)&&entrada->nodo==entradaA->nodo;
	}
	if(entradaA->etapa==Transformacion){
		trabajoTerminado(mismoNodo);
		if(trabajoTerminadoB){
			moverAUsados(mismoNodo);
			Entrada reducLocal;
			darDatosEntrada(&reducLocal);
			darPathTemporal(&reducLocal.pathTemporal,'l');
			reducLocal.etapa=ReducLocal;
			struct __attribute__((__packed__)){//para que no use relleno
				int32_t nodo;
				char* temp;
			}dato;
			dato.nodo=reducLocal.nodo;
			dato.temp=reducLocal.pathTemporal;
			mensajeEnviar(reducLocal.masterid,ReducLocal,&dato,sizeof dato);
			list_add(tablaEstados,&reducLocal);//mutex
		}
	}else if(entradaA->etapa==ReducLocal){
		trabajoTerminado(mismoJob);
		if(trabajoTerminadoB){
			moverAUsados(mismoJob);
			Entrada reducGlobal;
			darDatosEntrada(&reducGlobal);
			darPathTemporal(&reducGlobal.pathTemporal,'g');
			reducGlobal.etapa=ReducGlobal;
			int32_t nodoMenorCarga=entradaA->nodo;
			int menorCargaI=100; //
			void menorCarga(Worker* worker){
				if(worker->carga<menorCargaI)
					nodoMenorCarga=worker->nodo;
					menorCargaI=worker->carga;
			}
			list_iterate(workers,menorCarga);
			struct __attribute__((__packed__)){
				int32_t nodo;
				char* temp;
			}dato;
			dato.nodo=reducGlobal.nodo;
			dato.temp=reducGlobal.pathTemporal;
			mensajeEnviar(reducGlobal.masterid,ReducGlobal,&dato,sizeof dato);
			list_add(tablaEstados,&reducGlobal);//mutex
		}
	}else{
		list_add(tablaUsados,list_remove_by_condition(tablaEstados,mismoJob));
		mensajeEnviar(entradaA->masterid,Cierre,nullptr,0);
	}
}

void dibujarTablaEstados(){
	if(list_is_empty(tablaEstados))
		return;
	pantallaLimpiar();
	puts("Job    Master    Nodo    Bloque    Etapa    Temporal    Estado");
	void dibujarEntrada(Entrada* entrada){
		char* etapa,*estado;
		switch(entrada->etapa){
		case Transformacion: etapa="transformacion"; break;
		case ReducLocal: etapa="reduccion local"; break;
		default: etapa="reduccion global";
		}
		switch(entrada->estado){
		case EnProceso: estado="en proceso"; break;
		case Error: estado="error"; break;
		default: estado="terminado";
		}
		printf("%d     %d     %d     %d     %s     %s    %s",
				entrada->job,entrada->masterid,entrada->nodo,entrada->bloque,
				etapa,entrada->pathTemporal,estado);
	}
	list_iterate(tablaUsados,dibujarEntrada);
	list_iterate(tablaEstados,dibujarEntrada);
}

void darPathTemporal(char** ret,char pre){
	//mutex
	static char* anterior;
	static char agregado;
	char* temp=temporal_get_string_time();
	*ret=malloc(TEMPSIZE); //12
	int i,j=1;
	*ret[0]=pre; //creo que la precedencia esta bien
	for(i=0;i<12;i++){
		if(temp[i]==':')
			continue;
		*ret[j]=temp[i];
		j++;
	}
	*ret[10]='0';
	*ret[11]='\0';
	if(stringIguales(*ret,anterior))
		agregado++;
	else
		agregado='0';
	*ret[10]=agregado;
	anterior=string_duplicate(*ret); //leak?
}





//wat is dis
//void notificadorInformar(Socket unSocket) {
//	char buffer[BUF_LEN];
//	int length = read(unSocket, buffer, BUF_LEN);
//	int offset = 0;
//	while (offset < length) {
//		struct inotify_event *event = (struct inotify_event *) &buffer[offset];
//		if (event->len) {
//		if (event->mask & IN_MODIFY) {
//		if (!(event->mask & IN_ISDIR)) {
//		if(strcmp(event->name, "ArchivoConfig.conf"))
//			break;
//		ArchivoConfig archivoConfig = config_create(RUTA_CONFIG);
//		if(archivoConfigTieneCampo(archivoConfig, "RETARDO_PLANIFICACION")){
//			int retardo = archivoConfigEnteroDe(archivoConfig, "RETARDO_PLANIFICACION");
//				if(retardo != configuracion->retardoPlanificacion){
//					puts("");
//					log_warning(archivoLog, "[CONFIG]: SE MODIFICO EL ARCHIVO DE CONFIGURACION");
//					configuracion->retardoPlanificacion = retardo;
//					log_warning(archivoLog, "[CONFIG]: NUEVA RUTA METADATA: %s\n", configuracion->retardoPlanificacion);
//
//				}
//				archivoConfigDestruir(archivoConfig);
//		}
//		}
//		}
//		}
//		offset += sizeof (struct inotify_event) + event->len;
//		}
//
//	//Esto se haria en otro lado
//
//	//inotify_rm_watch(file_descriptor, watch_descriptor);
//		//close(file_descriptor);
//}
