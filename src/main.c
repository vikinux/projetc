#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct pile
{
	char * valeur;
	struct pile *next;
} pile ;

void supr(pile *l)
{
    pile *c;
    while(l!= NULL)
    {
        c = l;
        l = l -> next;
        free(c);
    }
}


void view(pile *p){
	p=p->next;
	while(p)
	{
		printf(" %s\n",p->valeur);
		p = p->next;
	}
}


static int callback(void *NotUsed, int argc, char **argv, char **azColName){
	int i;
	if(NotUsed){}
	for(i=0; i<argc; i++){
		printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	}
	return 0;
}

static int callback_cal(void * mapile, int argc, char **argv, char **azColName){
	

	pile *p = (pile *) mapile;

	pile *element = malloc(sizeof(pile));
	if(!element) exit(EXIT_FAILURE);     /* Si l'allocation a échouée. */
	element->valeur =  strdup(argv[0]);
	element->next = p->next;
	p->next = element;
	p = element;      


	mapile = (void *) p;
	

	return 0;
}


sqlite3 * createDataBase( sqlite3 * db, char * dbName){
	int rc;
	rc =  sqlite3_open_v2(dbName,&db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,NULL);
	if(rc){
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
	}
	printf("\n Creation de la database %s \n\n",dbName);
	return db;
}

int createCommande(sqlite3 * db, char * commande ){
	int rc=-1;
	char *zErrMsg = 0;
	rc = sqlite3_exec(db, commande,callback,0, &zErrMsg);
	if( rc!=SQLITE_OK ){
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}

	return rc;
}

FILE * ouverture(char * nomFichier){

	FILE * fichier;

	fichier = fopen(nomFichier,"r");

	if(fichier == NULL){
		printf("\nImpossible d'ouvrir le fichier log \n\n");
	}
	return fichier;
}

long long int date( char * line, int thread,struct sqlite3 *db, int nbRun, int previous){

	char * token = malloc(sizeof(char)*1024);
	int jour;
	int mois;
	int ans,heure,min,sec,up_d,up_h,up_m,up_s;
	long long int uptime;
	long long int uphour;
	char * tmp =  malloc(sizeof(char)*1024);

	sscanf(line, "Date: %d/%d/%d -- %d:%d:%d (uptime: %dd, %dh %dm %ds) \n", &jour, &mois ,&ans,&heure,&min,&sec,&up_d,&up_h,&up_m,&up_s);

	uptime = (up_s) +(up_m *60)+(up_h *60*60)+(up_d *24*60*60);
	uphour = (sec) +(min *60)+(heure *60*60);

	sprintf(tmp, "%d/%d/%d",jour,mois,ans);

	if(uptime < previous)
		nbRun ++;

	sprintf(token,"INSERT INTO info values(\"%s\",%lld,%lld,%d,%d);",tmp,uphour,uptime,thread,nbRun);

	createCommande(db, token);

	free(token);
	free(tmp);

	return uptime;
}

void param(char * line, int nbthread, struct sqlite3 *db, int nbRun){

	char * token = malloc(sizeof(char)*1024);
	char * token2 =  malloc(sizeof(char)*1024);
	char * token3 =  malloc(sizeof(char)*1024);
	char * commande = malloc(sizeof(char)*1024);


	sscanf(line, " %s | %s | %s \n", token, token2, token3);

	sprintf(commande,"INSERT INTO param values(\"%s\",\"%s\",\"%s\",%d,%d);",token,token2,token3,nbthread,nbRun);

	createCommande(db, commande);

	free(token);
	free(token2);
	free(token3);
	free(commande);
}

char * parseDuFichier( FILE * fichier, struct sqlite3 *db){

	char * ligne = malloc(sizeof(char)*1024);
	int nbThread = 0;
	int nbRun = 0;
	int nbThreadTotal=0;
	long long int previous_runtime = 10;
	long long int runtime = 0;
	char * resultat = malloc(sizeof(char)*1024);


	while(fscanf(fichier, "%[^\n]\n", ligne) > 0){


		//	printf("%s \n",ligne);
		if(ligne[0] == '-'){
			nbThread ++;
			fscanf(fichier, "%[^\n]\n", ligne);
			//	runtime = date(ligne,nbThread,db, nbRun);

			if(runtime >= previous_runtime){
				previous_runtime = runtime;
			}
			else{
				nbRun ++;
				previous_runtime = runtime;
				nbThreadTotal += nbThread;
				nbThread = 0;

			}


			runtime = date(ligne,nbThread,db, nbRun, previous_runtime);


			fscanf(fichier, "%[^\n]\n", ligne) ;
			fscanf(fichier, "%[^\n]\n", ligne) ;
			fscanf(fichier, "%[^\n]\n", ligne) ;
		}
		else {
			param(ligne, nbThread, db, nbRun);
		}

	}
	nbThreadTotal += nbThread;


	free(ligne);
	sprintf(resultat,"T:%d R:%d",nbThreadTotal, nbRun);
	return resultat;

}

int requeteDureeRun(struct sqlite3 *db, int nbRun){

	int uptime = 0; //malloc(sizeof(char)*1024);
	int rc;
	pile *pileTime = NULL;
	char *zErrMsg = 0;
	char * commande = malloc(sizeof(char)*1024);
	

	pile *element = malloc(sizeof(pile));
	if(!element) exit(EXIT_FAILURE);     /* Si l'allocation a échouée. */
	element->valeur =  NULL;
	element->next =  NULL;
	pileTime = element;       /* Le pointeur pointe sur le dernier élément. */
	sprintf(commande, "SELECT uptime FROM info WHERE run = %d;",nbRun);
	rc = sqlite3_exec(db,commande ,callback_cal,element, &zErrMsg);
	if( rc!=SQLITE_OK ){
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}
	
	pileTime = pileTime->next;

	uptime = atoi(pileTime->valeur);
		


	printf("durée du run %d = %d secondes \n", 1, uptime);
	supr(pileTime);

	return uptime;
}

void requetedebutfin(struct sqlite3 *db, int nbRun){

	char * debut = malloc(sizeof(char)*1024);
	char * fin = malloc(sizeof(char)*1024);
	int rc;
	pile *pileDate = NULL;
	char *zErrMsg = 0;

	pile *element = malloc(sizeof(pile));
	if(!element) exit(EXIT_FAILURE);     /* Si l'allocation a échouée. */
	element->valeur =  NULL;
	element->next =  NULL;
	pileDate = element;       /* Le pointeur pointe sur le dernier élément. */


	char * commande = malloc(sizeof(char)*1024);
	sprintf(commande, "SELECT date FROM info WHERE run = %d;",nbRun);


	rc = sqlite3_exec(db,commande ,callback_cal,element, &zErrMsg);
	if( rc!=SQLITE_OK ){
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}


	printf("date %s \n",pileDate->valeur);
	pileDate = pileDate->next;

//	view(p);

//	printf("test");
	fin = pileDate->valeur;
	
	while(pileDate){
		if(pileDate->next == NULL){	
			debut = pileDate->valeur;
			}
		pileDate = pileDate->next;
	
	}

	printf("debut : %s fin : %s \n", debut, fin);
	supr(pileDate);

}

 long long int somme(pile * p){

	long long int resultat = 0;
	p = p->next;
	while(p){
		resultat = resultat + atoi(p->valeur);
		p = p->next;
	
	}
			
	return resultat;
}
long long int nbdrop(struct sqlite3 *db, int nbRun){

	int rc = 0;
	pile *pileDrop = NULL;
	char *zErrMsg = 0;
	long long int resultat = 0;

	pile *element = malloc(sizeof(pile));
	if(!element) exit(EXIT_FAILURE);     /* Si l'allocation a échouée. */
	element->valeur =  NULL;
	element->next =  NULL;
	pileDrop = element;       /* Le pointeur pointe sur le dernier élément. */

	char * commande = malloc(sizeof(char)*1024);
	sprintf(commande, "SELECT col3  FROM param WHERE run = %d AND col1 = \"capture.kernel_drops\";",nbRun);


	rc = sqlite3_exec(db,commande ,callback_cal,element, &zErrMsg);
	if( rc!=SQLITE_OK ){
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}

	resultat = somme(pileDrop);
	supr(pileDrop);


	return resultat;
}

long long int nbpacket(struct sqlite3 *db, int nbRun){

	int rc = 0;
	pile *pilePacket = NULL;
	char *zErrMsg = 0;
	long long int resultat = 0;

	pile *element = malloc(sizeof(pile));
	if(!element) exit(EXIT_FAILURE);     /* Si l'allocation a échouée. */
	element->valeur =  NULL;
	element->next =  NULL;
	pilePacket = element;       /* Le pointeur pointe sur le dernier élément. */

	char * commande = malloc(sizeof(char)*1024);
	sprintf(commande, "SELECT col3  FROM param WHERE run = %d AND col1 = \"capture.kernel_packets\";",nbRun);


	rc = sqlite3_exec(db,commande,callback_cal,element, &zErrMsg);
	if( rc!=SQLITE_OK ){
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}

	resultat = somme(pilePacket);
	supr(pilePacket);


	return resultat;
}

void calculRatioDrop(struct sqlite3 *db, int nbRun){

	long long int drops = nbdrop(db, nbRun);
	long long int packets = nbpacket(db, nbRun);
	
	
	int resultat = 0;
	
	resultat = drops * 100 / packets;



	printf("ratio kernel drop pour run %d = %d % !\n",1,resultat);
//view( mapile);
	
	}
	
void moyennedroprun(struct sqlite3 * db, int nbRun){

	long long int drops = 0;
	int duree = 0;	
	long long int resultat = 0;
	
	drops = nbdrop(db, nbRun);
	duree = requeteDureeRun(db,nbRun);
	resultat = resultat +drops / duree;
	
	printf("drop/sec = %lld \n", resultat);
	
	
}
	
int main(int argc, char **argv){


	int opt; 
	struct sqlite3 *db = NULL;
	FILE * fichier;
	int nbRun, nbThread;
	fichier = ouverture("stat.log");
	
	
	while( (opt = getopt(argc, argv, "hcds:")) !=-1){
	
		switch (opt){
		
			case 'h':
				printf("HELP");
			case 'c':
				db = createDataBase( db, argv[optind]);

				printf("  En cours ...\n");
				createCommande(db, "PRAGMA synchronous = OFF;");
				createCommande(db, "CREATE TABLE param (col1 char(50), col2 char(50), col3 int, thread int, run int);");
				createCommande(db, "CREATE TABLE info (date char(50),heure int, uptime int, thread int, run int);");
				sscanf(parseDuFichier(fichier, db),"T:%d R:%d",&nbThread,&nbRun);	
				printf("  Fini!  \n");
				break;
			case 'd':
				break;
			case 's':
				calculRatioDrop(db,nbRun);
				requetedebutfin(db,nbRun);
				requeteDureeRun(db,nbRun);
				moyennedroprun(db, nbRun);
		
		}
	
	
	}


	printf("  Fini!  \n");	
	return 0;
}
