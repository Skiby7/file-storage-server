---
author:
	- "Leonardo Scoppitto"
classoption: a4paper
documentclass: article
fontsize: 11pt
geometry: "left=2cm,right=2cm,top=2cm,bottom=2cm"
output:
	pdf_document:
		latex_engine: xelatex
title: Relazione file storage server
---
\pagenumbering{gobble}
\hypersetup{linkcolor=black}

\pagebreak
\renewcommand{\contentsname}{Indice}
\tableofcontents
\pagebreak
\pagenumbering{arabic}
\setcounter{page}{1}
# Introduzione

Il progetto è stato testato sulla macchina virtuale fornita dai docenti con 2 cores e **almeno** 2GB di ram.
È possibile consultare il repository di git al [seguente link](https://github.com/Skiby7/file-storage-server).
Sono state sviluppate delle parti opzionali quali:

* Algoritmi di rimpiazzamento più avanzati di quello `FIFO` (vedere le sezioni *3.5 Rimpiazzamento file*): quali `LRU`, `LFU` e una via di mezzo fra questi, che chiamerò per comodità `LRFU`

* L'opzione -x per il client

* La compressione dei file presenti nel server


# Compilazione ed esecuzione

Una volta scompattato l'archivio, sarà sufficiente spostarsi dentro la root del progetto, compilare il progetto con il comando `make` o `make -j` e dopodiché i file eseguibili si troveranno dentro `bin`. Inoltre, è possibile testare il progetto con i comandi `make testX` dove `X` è il numero del test che si vuole eseguire. 
Nel caso si volesse clonare il progetto direttamente da Github, è necessario usare l'opzione `--recursive` per scaricare anche il sorgente di zlib, necessario per la compilazione del progetto:
```
git clone https://github.com/Skiby7/file-storage-server --recursive
```
Inoltre, appena scaricato il progetto da Github, non sono presenti i file necessari per eseguire i test, perciò la compilazione richiederà qualche secondo in più, in quanto verrà eseguito uno script per generare i file di testo. 

## Makefile targets

Oltre a quelli definiti nella specifica, è possibile usare anche i seguenti target:

* `clean`: rimuove le librerie statiche, gli eseguibili e eventuali altri file generati durante la compilazione

* `clean_files`: rimuove i file usati per i test

* `clean_test`: rimuove i file generati dai test

* `clean_all`: esegue `clean` e `clean_files`

* `gen_files`: genera i file testuali usati per i test

* `test3_un`, `test3_quiet`, `test3_un_quiet`: per eseguire il test 3 rispettivamente con la compressione disabilita, con il parametro `TUI` del file di configurazione disabilitato (vedere le sezioni *3.1 File di configurazione* e *6.2 Interfaccia testuale*) e, infine, con la compressione e `TUI` disabilitati. 

# Server

L'applicazione `server` consiste in un programma multi-threaded che gestisce più richieste contemporanee di connessione da parte di client diversi. I client comunicano col server tramite un'API, la cui implementazione si trova in `fssApi.c` e dopodiché vengono serviti da un worker del server il quale esegue un'operazione per poi rimettersi in attesa.

## File di configurazione

Il file di configurazione, passato come argomento da linea di comando all'avvio, è un generico file di testo contenente le seguenti parole chiave seguiti dal carattere `':'` : `WORKERS`, `MAXMEM`, `MAXFILES`, `SOCKNAME`, `LOGFILE`, `TUI`, `REPLACEMENT_ALGO`, `COMPRESSION` e `COMPRESSION_LEVEL`.
`SOCKNAME` è il nome che si vuole dare al socket, che verrà creato in `/tmp`. Se si imposta un path, sia assoluto, che relativo, verrà considerato solo il nome del file (vedere `man 3 basename`).

Le voci `LOGFILE`, `REPLACEMENT_ALGO`, `COMPRESSION` e `TUI` sono opzionali:

* Se `LOGFILE` non è definito non verrà prodotto il file di log. Per specificare il file di log si può usare sia un path relativo che assoluto, ma in entrambi i casi non verranno create le parent directory.

* `REPLACEMENT_ALGO` può essere impostato sui valori `FIFO`, `LRU`, `LFU`, `LRFU`. Se non specificato, viene usato l'algoritmo `FIFO`.

* `TUI` (l'acronimo di *textual user interface*), indica se si vuole stampare sullo standard output un sommario della configurazione e visualizzare in tempo reale le operazioni che avvengono nel server. Se vale `y`, l'output verrà prodotto, altrimenti no.  

* Per attivare la compressione è necessario settare la voce `COMPRESSION` su `y`, specificando il livello di compressione settando `COMPRESSION_LEVEL` su un valore compreso fra 0 e 9 (dove 9 è il massimo della compressione e 0 è il minimo) altrimenti, se non si specifica, verrà automaticamente impostato su 6.

Se manca una voce non opzionale o l'input di una voce non opzionale non è valido, il server non parte. Inoltre è possibile inserire linee vuote e definire commenti con il carattere `#` (tutto quello che segue `#` verrà ignorato). L'implementazione del parser è contenuta in `parser.c` e `parser.h`.


## Polling

In `server.c` è possibile consultare l'implementazione del thread Master e del signal handler. Una volta fatto partire il server, il thread Master si mette in ascolto di `com_fd` (il quale viene creato con dimensione `DEFAULTFDS` e riallocato dinamicamente mano a mano che i client connessi aumentano) con una `poll()`. Le prime 3 posizioni di `com_fd` sono riservate come segue:

0. Il socket su cui eseguire l'`accept()`.

1. La pipe sulla quale i thread Worker restituiscono i file descriptor dei client ancora connessi (`good_fd_pipe`) .

2. La pipe su cui, sempre i thread Worker, restituiscono i file descriptor dei client che hanno effettuato la disconnessione, che sono crashati o che hanno un fd corrotto (`done_fd_pipe`), così da chiuderli e aggiornare il numero di client connessi (`clients_active`).

## Signal handling e terminazione

La gestione dei segnali è demandata a un thread dedicato: all'avvio del server vengono mascherati tutti i segnali ad eccezione di `SIGSEGV` e viene poi avviato il thread `signal_handler_thread` sulla routine `sig_wait_thread()`. Qui, il thread si mette in ascolto dei seguenti segnali:

* `SIGINT` e `SIGQUIT`: alla ricezione di uno di questi due segnali, il flag globale `abort_connections` viene settato su `true`, mentre `can_accept` viene settato su `false`. Dopodiché viene inviata la stringa *termina* al thread Master tramite `good_fd_pipe`, così che, nel caso fosse in attesa sulla `poll()`, si svegli e termini.

* `SIGHUP`: in questo caso il thread setta solo `can_accept` su `false`, in modo che il Master non accetti più connessioni e attenda che il counter dei client attivi arrivi a `0` per poi terminare.

* `SIGUSR1`: questo segnale è dedicato alla terminazione di `signal_handler_thread`, infatti gli viene inviato dal thread Master prima della chiusura del programma.

Il thread Master, una volta uscito dal loop principale, imposta `abort_connections` su `true`, svuota la coda dei lavori e la riempie con il valore `-2` per poi svegliare i thread Worker e farli così terminare. Dopodiché sveglia il thread `use_stat_thread`, che termina quando `abort_connections` è impostato su `true`, invia il segnale `SIGUSR1` al `signal_handler_thread`, logga gli ultimi dati ed infine libera la memoria allocata dinamicamente.

## Storage

L'implementazione dello storage vero e proprio può essere consultata nei file `file.c` e `file.h`, dove sono definite le funzioni che i workers possono usare per modificare i dati all'interno del server. Ho deciso di implementare lo storage con una tabella hash con liste di trabocco, la cui dimensione è 1,33 volte il numero massimo di file che il server può gestire, così da mantenere il fattore di carico sotto il 75% e avere delle buone prestazioni (come da letteratura). Per il calcolo dell'hash ho usato l'algoritmo di Peter Jay Weinberger (la cui [implementazione](http://didawiki.cli.di.unipi.it/lib/exe/fetch.php/informatica/sol/laboratorio21/esercitazionib/icl_hash.tgz) è stata fornita a laboratorio). \
Le strutture che definiscono la memoria del server, `fss_file_t` e `fss_storage_t`, sono consultabili in `file.h`:

* `fss_file_t` contiene, oltre ai metadati e ai dati del file stesso, due mutex, una *condition variable* e due contatori `readers` e `writers` con i quali ho implementato la procedura *lettore-scrittore* vista a lezione. La struttura contiene, inoltre, il timestamp dell'ultimo accesso, il timestamp della creazione e un contatore di utilizzo `use_stat`, che vedremo in seguito nella sezione dedicata all'algoritmo di rimpiazzamento.

* `fss_storage_t` contiene, oltre alla tabella hash, la dimensione della tabella stessa e i parametri di configurazione, una mutex per garantire l'accesso mutualmente esclusivo a tutti i campi dello storage e delle variabili in cui salvare i dati per generare le statistiche al termine dell'esecuzione.

## Rimpiazzamento dei file

Come detto prima, ogni file ha un contatore `use_stat` il quale viene impostato a 16 al momento della creazione e varia fra 0 e 32. Viene quindi aumentato di una unità ogni volta che il file viene letto, scritto (in questo caso di 2 unità) o bloccato e viene decrementato da un thread dedicato, il quale viene svegliato in seguito a una scrittura o alla creazione di un nuovo file all'interno del server.
Inoltre, ogni volta che si accede a un file in lettura o in scrittura, viene aggiornato il suo campo `last_access`.
Una volta che si deve liberare dello spazio in memoria, viene chiamata la funzione `select_victim()`, che non fa altro che scorrere tutta la tabella e copiare i metadati dei file (eccetto del file che ha causato la chiamata di `select_victim()`) in un array di tipo `victim_t`. Questo array viene poi ordinato con `qsort()` e, finché non viene liberata abbastanza memoria, si elimina il file con pathname uguale a `victims[i].pathname`.
Di seguito la funzione `compare()` utilizzata in `qsort()`:

```c
static int compare(const void *a, const void *b) {
	victim_t a1 = *(victim_t *)a, b1 = *(victim_t *)b; 
	switch (server_storage.replacement_algo){
		case FIFO:
			return a1.created_time - b1.created_time;
		case LRU: 
			return a1.last_access - b1.last_access;
		case LFU:
			return a1.use_stat - b1.use_stat;
		case LRFU:
			if((a1.use_stat - b1.use_stat) != 0) return a1.use_stat - b1.use_stat;
			else return a1.last_access - b1.last_access;
	}
	return a1.created_time - b1.created_time;
}
```
Come si può vedere, nel caso di una politica `FIFO`, ordino gli elementi solo in base al tempo di creazione, con una politica `LRU` in base all'ultimo accesso effettuato, con una politica `LFU` in base al valore di `use_stat`, mentre con una politica `LRFU` si ordina prima in base a `use_stat` e, se questo è uguale per i due file, si confronta chi è stato usato più recentemente.
Il thread `use_stat_thread`, che esegue la routine `use_stat_update()`, inoltre, si occupa di sbloccare eventuali file rimasti inutilizzati con `use_stat` uguale 0, passando la lock ai client in attesa. 


# Client

Il client consiste in un programma single-threaded creato per interfacciarsi col server: una volta avviato, il client leggerà con `getopt()` tutti le opzioni passate come argomento da riga di comando e le salverà in una coda, per poi connettersi al server ed eseguire i comandi. L'implementazione del client è contenuta in `client.c`, `client.h`, `work.c` e `work.h`. 
Come da specifica, ogni file è identificato dal suo path assoluto (sul quale viene calcolato l'hash), pertanto, per evitare ambiguità, tutte le operazioni del client che prendono in input un file richiedono un path assoluto, altrimenti l'operazione fallirà. Fanno eccezione le opzioni `-d`, `-D` e `-w`, poiché prendono in input una cartella e se ne occuperà il client di trasformare il path da relativo ad assoluto. Inoltre, le opzioni che prevedono il salvataggio dei file sul disco (`-d` o `-D`), ricostruiscono l'albero delle directory a partire dal nome del file, utilizzando come *root* la cartella specificata.

# Comunicazione client-server

La comunicazione fra il client (C) e il server (S) avviene tramite il protocollo richiesta-risposta come segue:

1. C: Invia la lunghezza della richiesta

2. S: Il thread Master invia il file descriptor del client al thread Worker che legge la lunghezza della richiesta, invia un byte di *acknowledge* al client e alloca la memoria per leggere la richiesta

3. C: Riceve l'acknowledge, invia il pacchetto con la richiesta serializzata e si mette in attesa

4. S: Riceve la richiesta, la processa, serializza la risposta e invia la dimensione della risposta

5. C: Legge la lunghezza della risposta invia un byte di acknowledge e alloca la memoria per leggere la risposta, dopodiché la processa

6. S: Il thread Worker invia il file descriptor del client al thread Master e si rimette in attesa




## Comandi

Ho deciso di condensare sia i comandi in un byte per semplicità: al momento avendo solo 8 operazioni possibili, a ogni bit del campo `command` corrisponde un'operazione e, nel caso se ne volessero implementare altre, basterebbe usare una combinazione di bit per identificare quelle nuove.

| Operazione 	| Valore 	| Descrizione                                                         	|
|------------	|:--------:	|---------------------------------------------------------------------	|
| OPEN       	| 0x01   	| Operazione di apertura file                                         	|
| CLOSE      	| 0x02   	| Operazione di chiusura file                                         	|
| READ       	| 0x04   	| Operazione di lettura file                                          	|
| READ_N       	| 0x08   	| Operazione di lettura di N file casuali                              	|
| WRITE      	| 0x10   	| Operazione di scrittura nuovo file                                  	|
| APPEND     	| 0x20   	| Operazione di scrittura in append a un file                         	|
| REMOVE     	| 0x40   	| Operazione di rimozione file                                        	|
| SET_LOCK   	| 0x80   	| Operazione di lock/unlock file:                                     	|
|            	|        	| se il campo  flag  della richiesta vale  `O_LOCK`,                  	|
|            	|        	| viene eseguita l'operazione di lock,  altrimenti si esegue l'unlock 	|

I flag `O_CREATE` e `O_LOCK` hanno, rispettivamente, il valore `0x01` e `0x02`

\pagebreak

## Errori

Ho deciso di definire dei codici di errore per estendere gli errori riportati in `errno.h` e dare una spiegazione più specifica del problema, come ad esempio una mancata lock o una open ripetuta. Gli errori specifici vengono inviati al client nel campo `code` della risposta e si trovano alla posizione 0 (vedere sezione successiva). Di seguito la tabella: 

| Codice                 	| Valore 	| Descrizione                                        	|
|------------------------	|:--------:	|----------------------------------------------------	|
| FILE_OPERATION_SUCCESS 	|  `0x01` 	| L'operazione è stata completata con successo       	|
| FILE_OPERATION_FAILED  	|  `0x02`  	| Si è verificato un errore e                        	|
|                        	|        	| l'operazione non è stata completata                	|
| FILE_ALREADY_OPEN      	|  `0x04`  	| Il file che si sta cercando di aprire è già aperto 	|
| FILE_ALREADY_LOCKED    	|  `0x08`  	| Il file che si sta cercando di bloccare è          	|
|                        	|        	| già bloccato dal client chiamante                  	|
| FILE_LOCKED_BY_OTHERS  	|  `0x10`  	| Il file che si sta cercando di bloccare            	|
|                        	|        	| o modificare è bloccato da un altro client         	|
| FILE_NOT_LOCKED        	|  `0x20`  	| Il file non è stato bloccato prima di              	|
|                        	|        	| un'operazione che richiede la lock                 	|
| FILE_NOT_OPEN        		|  `0x40`  	| Il file non è stato aperto 							|
| STOP        				|  `0x80`  	| Segnale di stop per la `READ_N`						|


## `client_request` e `server_response`

Sia il server che il client utilizzano le seguenti strutture per organizzare le richieste e le risposte da inviare:

```c
typedef struct client_request_{
	unsigned int client_id;	
	unsigned char command;	
	unsigned char flags;	
	int files_to_read; 		
	unsigned int pathlen;	
	char *pathname;			
	unsigned long size;		
	unsigned char* data;
} client_request;

typedef struct server_response_{
	unsigned int pathlen;
	char *pathname;
	unsigned char has_victim;
	unsigned char code[2];
	unsigned long size;
	unsigned char* data;
} server_response;
```

I campi in comune per la richiesta e la risposta sono: `pathlen` che indica la lunghezza della stringa `pathname` (compreso `\0`) e `size` il quale indica la lunghezza del campo `data`, che a sua volta contiene i dati (non compressi) del file inviato/ricevuto. 

\pagebreak

Per quanto riguarda `client_request`, partendo dall'alto si trova:

* `client_id`: ogni client è identificato dal `PID` del suo processo

* `command`: comando da eseguire (vedere sezione *5.1 Comandi*)

* `flags`: assume i valori `O_LOCK` e/o `O_CREATE`

* `files_to_read`: numero di file da leggere per l'operazione *readNFile* 

Mentre, per `server_response` abbiamo:

* `has_victim`: indica se dopo la prima risposta il client si deve rimettere in attesa per ricevere i file eliminati in seguito a una scrittura. Vale 0 o 1

* `code[2]`: all'indice 0 si ha l'esito dell'operazione (vedere sezione precedente), mentre all'indice 1 si trova un eventuale valore di errore fra quelli definiti in `errno.h` e `errno-base.h`

## Serializzazione e deserializzazione

Tutti i dati, sia la lunghezza del pacchetto, che il pacchetto, vengono inviati come array di `unsigned char`. L'implementazione della conversione da `uint32_t` e `uint64_t` ad array di byte e la serializzazione/deserializzazione delle richieste possono essere consultate in `serialization.c` e `serialization.h`.
Ho deciso di serializzare le richieste e le risposte piuttosto che inviare campo per campo leggendone e scrivendone uno alla volta, in modo da utilizzare un numero minore di read/write (una per la dimensione del pacchetto e una per il pacchetto stesso). Inoltre questo approccio permette di aggiornare facilmente il programma per comunicare attraverso la rete, piuttosto che in locale.
In `serialization.c` e `serialization.h` si trova l'implementazione delle funzioni usate per la serializzazione.

\pagebreak

# Parti aggiuntive

## Compressione

Ho implementato la compressione dei file usando la libreria open-source `zlib`. Nel file di configurazione del server, questa opzione può essere abilitata impostando il parametro `COMPRESSION` su `y` e si può impostare il livello di compressione su un valore compreso fra 0, che è il minimo e 9, che è il massimo (il livello 0 è usato da `zlib` per creare un archivio di più file senza però comprimerli).
La libreria è stata compilata con il flag `--const` e come libreria statica.

### Copyright notice

Direttamente dal README del [repository](https://github.com/madler/zlib) ufficiale:
```
 (C) 1995-2017 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.
  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu
```

## Interfaccia testuale

Per rendere più interattivo il test3 e per avere qualche informazione utile durante l'uso del server, ho implementato una semplice interfaccia testuale con la quale avere una panoramica sia della configurazione del server, che dell'uso delle risorse del server. Per ridurre al minimo l'impatto sulle performance, invece che far ristampare a ogni thread Worker l'intera schermata ogni volta che esegue un'operazione che modifichi i file in numero o in dimensione, ho preferito utilizzare un thread dedicato, il quale si mette in ascolto con una `poll()` su una pipe. La pipe viene utilizzata dai Worker per notificare l'inizio e la fine di un'operazione di `READ`/`READ END` o di `WRITE`/`WRITE END`, così che il thread possa ridisegnare l'interfaccia in base all'azione eseguita. L'accesso alle informazioni dello storage, quindi la lock della mutex globale dello storage, viene effettuata solo quando viene ricevuta una `WRITE END`. 

## Opzione -x

Ho introdotto, per il client, l'opzione `-x` da usare dopo `-w`: l'opzione permette di sbloccare tutti i file caricati dalla cartella passata come parametro, senza doverli sbloccare manualmente uno ad uno.
