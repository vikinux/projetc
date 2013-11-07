#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>


typedef struct pile
{
	char * valeur;
	struct pile *next;
} pile ;


void view(pile *p){
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
	int i;

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

//void requetedebutfin(pile * p){

//	char * debut = malloc(sizeof(char)*1024);
//	char * fin = malloc(sizeof(char)*1024);


//	view(p);

//	printf("test");
//	fin = p->valeur;
	
//	while(p->prec){
//		printf("%s \n",p->valeur);
//		debut = p->valeur;
//	}

//	printf("debut : %s fin : %s", debut, fin);

//}

void calcul(struct sqlite3 *db){

	int rc = 0;
	pile *mapile = NULL;
	char *zErrMsg = 0;

	pile *element = malloc(sizeof(pile));
	if(!element) exit(EXIT_FAILURE);     /* Si l'allocation a échouée. */
	element->valeur =  NULL;
	element->next =  NULL;
	mapile = element;       /* Le pointeur pointe sur le dernier élément. */


	rc = sqlite3_exec(db,"SELECT uptime FROM info WHERE run = 1;" ,callback_cal,element, &zErrMsg);
	if( rc!=SQLITE_OK ){
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}


//	requetedebutfin( (pile *) mapile);
//
	view( mapile);
//
//

}
int main(int argc, char **argv){

	struct sqlite3 *db = NULL;
	FILE * fichier;
	int nbRun, nbThread;
	fichier = ouverture("stat.log");



	if(argc > 1) {
		db = createDataBase( db, argv[1]);
		createCommande(db, "PRAGMA synchronous = OFF;");
		createCommande(db, "CREATE TABLE param (col1 char(50), col2 char(50), col3 int, thread int, run int);");
		createCommande(db, "CREATE TABLE info (date char(50),heure int, uptime int, thread int, run int);");
	}
	printf("  En cours ...\n");
//		sscanf(parseDuFichier(fichier, db),"T:%d R:%d",&nbThread,&nbRun);	


//		createCommande(db, "SELECT uptime FROM info WHERE run = 2;");


	printf("  Fini!  \n");	

	calcul(db);

//	printf("  Fini!  %d %d \n",nbRun, nbThread);	
	return 0;
}
