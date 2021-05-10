Il server non farà mai write su una directory del disco locale.
Si usa solo il path assoluto
Tutte le richieste lato client vengono fatte tramite un solo fd.
Ogni richiesta è seguita da una risposta, in ogni caso.
I worker prendono le richieste da una lista, ho un pool di worker a cui assegnare cose. Quando uno non è busy, chiappa una richiesta e la soddisfa.
Far aprire config da argv
Le api devono essere dentro una lib
La lib deve essere fatta ammodo perché tutti la usano -> rientrante
server blob di write
scrittura in binario

riascolta a 2:19:00