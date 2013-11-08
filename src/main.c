/* author : MULLER Rémy */
/* date : 08/11/2013    */


#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/****************************Structure of the linked list*******************/
typedef struct pile
{
	char * valeur;
	struct pile *next;
} pile ;

/****************************Suppression of the linked list******************/

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

/*****************************show linked list values***************************/

void view(pile *p){
	p=p->next;
	while(p)
	{
		printf(" %s\n",p->valeur);
		p = p->next;
	}
}

/******************************INSERTION : database insertion's callback*************/

static int callback(void *NotUsed, int argc, char **argv, char **azColName){ 
	int i;
	if(NotUsed){}
	for(i=0; i<argc; i++){
		printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");   // Show the result of the SQLITE3 query
	}
	return 0;
}
/********************STATS : Insertion of the sqlite request in the linked list. Insertion on the begining of the list******/

static int callback_cal(void * mapile, int argc, char **argv, char **azColName){

	pile **p = (pile **) mapile;

	pile *element = malloc(sizeof(pile));
	if(!element) exit(EXIT_FAILURE);    
	element->valeur =  strdup(argv[0]);			// Save the result of the SQLITE3 query in the beginning of the list 
	element->next = (*p);					// Callback_cal is executed for each line of the database response
	*p = element;      

	return 0;
}

/*****************************INSERTION AND STATS : Create or open the database***********************************************/

sqlite3 * createDataBase( sqlite3 * db, char * dbName){
	int rc;
	rc =  sqlite3_open_v2(dbName,&db,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,NULL);  // Open the database and create it if it doesn't exist
	if(rc){
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
	}
	printf("\n Creation/Ouverture de la database %s \n\n",dbName);
	return db;
}

/****************************INSERTION : Creation of sqlite queries************************************************/

int createCommande(sqlite3 * db, char * commande ){
	int rc=-1;
	char *zErrMsg = 0;
	rc = sqlite3_exec(db, commande,callback,0, &zErrMsg);		// Execute the commande and send the response in the callback
	if( rc!=SQLITE_OK ){
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}

	return rc;
}

/**************************************INSERTION : Opening of stat.log*****************************/

FILE * ouverture(char * nomFichier){

	FILE * fichier;

	fichier = fopen(nomFichier,"r");			// Open the file in read only mod 

	if(fichier == NULL){
		printf("\nImpossible d'ouvrir le fichier log \n\n");
	}
	return fichier;
}

/********************************INSERTION  : parsing run information********************************************/

long long int date( char * line, struct sqlite3 *db, int nbRun, int previous){

	char * token = malloc(sizeof(char)*1024);
	int jour;
	int mois;
	int ans,heure,min,sec,up_d,up_h,up_m,up_s;
	long long int uptime;
	long long int uphour;
	char * tmp =  malloc(sizeof(char)*1024);

	sscanf(line, "Date: %d/%d/%d -- %d:%d:%d (uptime: %dd, %dh %dm %ds) \n", &jour, &mois ,&ans,&heure,&min,&sec,&up_d,&up_h,&up_m,&up_s);	// Parse the line and save the wanted values 

	uptime = (up_s) +(up_m *60)+(up_h *60*60)+(up_d *24*60*60);		// convert the uptime into second
	uphour = (sec) +(min *60)+(heure *60*60);				// convert the hour into second
	sprintf(tmp, "%d/%d/%d %d:%d:%d",jour,mois,ans,heure,min,sec);		// Formats the date

	if(uptime < previous){							// detect if it's a new run
		nbRun ++;

		sprintf(token,"INSERT INTO info values(\"%s\",\"%s\",%lld,%lld,%d);",tmp, tmp ,uphour,uptime,nbRun); // Format the SQLITE3 query

		createCommande(db, token);

	}else{
		sprintf(token,"UPDATE info SET uptime = %lld WHERE run = %d;",uptime,nbRun);		// Format the SQLITE3 query
		createCommande(db, token);								// Create the SQLITE3 query
		sprintf(token,"UPDATE info SET datefin = \"%s\" WHERE run = %d;",tmp,nbRun);		// Format the SQLITE3 query
		createCommande(db, token);								// Create the SQLITE3 query
	}

	free(token);											//free the resource
	free(tmp);

	return uptime;
}

/******************************INSERTION : parsing dump information**********************************************/

void param(char * line, int nbDump, struct sqlite3 *db, int nbRun){

	char * token = malloc(sizeof(char)*1024);
	char * token2 =  malloc(sizeof(char)*1024);
	char * token3 =  malloc(sizeof(char)*1024);
	char * commande = malloc(sizeof(char)*1024);

	sscanf(line, " %s | %s | %s \n", token, token2, token3); // Parse the line and save the wanted values 

	sprintf(commande,"INSERT INTO param values(\"%s\",\"%s\",\"%s\",%d,%d);",token,token2,token3,nbDump,nbRun); // Format the SQLITE3 query

	createCommande(db, commande);			// Create the SQLITE3 query

	free(token);
	free(token2);					//free the resource
	free(token3);
	free(commande);
}

/*********************************INSERTION  : file reading. return the amount of dump and run*******************************************/

char * parseDuFichier( FILE * fichier, struct sqlite3 *db){

	char * ligne = malloc(sizeof(char)*1024);
	int nbDump = 0;
	int nbRun = 0;
	int i=0;
	int nbDumpTotal=0;
	long long int previous_runtime = 100;
	long long int runtime = 0;
	char * resultat = malloc(sizeof(char)*1024);


	while(fscanf(fichier, "%[^\n]\n", ligne) > 0){
		if(i == 0)
			createCommande(db,"BEGIN TRANSACTION");		// Tell the database that it has to wait before commiting our queries
									// Greatly increase the speed of the execution speed of the queries
		if(ligne[0] == '-'){
			nbDump ++;					// count the amount of Dump in the currently run
			fscanf(fichier, "%[^\n]\n", ligne);

			runtime = date(ligne,db, nbRun, previous_runtime);  // Parse the date line and return the last dump uptime
			if(runtime >= previous_runtime){			// Test if it's a new run
				previous_runtime = runtime;
			}
			else{
				nbRun++;
				previous_runtime = runtime;
				nbDumpTotal += nbDump;
				nbDump = 0;
	
			}
			fscanf(fichier, "%[^\n]\n", ligne) ;
			fscanf(fichier, "%[^\n]\n", ligne) ;
			fscanf(fichier, "%[^\n]\n", ligne) ;
		}
		else {
			param(ligne, nbDump, db, nbRun);			// Parse the parameters line
		}
		i++;
		if(i == 500){
			i = 0;
			createCommande(db,"COMMIT TRANSACTION");		// Commit all the queries every 500 queries
		}								// 500 is the optimized amount of queries before commiting
			
	}
	nbDumpTotal += nbDump;							// count the total of dump during the execution

	free(ligne);								// free the resource
	sprintf(resultat,"D:%d R:%d",nbDumpTotal, nbRun);
	return resultat;

}

/********************************STATS :Run duration********************************************/

int requeteDureeRun(struct sqlite3 *db, int nbRun){

	int uptime = 0; 
	int rc;
	pile *pileTime = NULL;
	char *zErrMsg = 0;
	char * commande = malloc(sizeof(char)*1024);


	pile *element = malloc(sizeof(pile));			// create an empty cell
	if(!element) exit(EXIT_FAILURE);    
	element->valeur =  NULL;
	element->next =  NULL;

	sprintf(commande, "SELECT uptime FROM info WHERE run = %d;",nbRun);

	rc = sqlite3_exec(db,commande ,callback_cal,&element, &zErrMsg);   //fill the linked list element with the query's result
	if( rc!=SQLITE_OK ){
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}

	pileTime = (pile *) element;		// we cast element into pile * because the callback change it into a void *

	uptime = atoi(pileTime->valeur);

	supr(pileTime);

	return uptime;
}

/******************************STATS : end and**********************************************/

void requetedebutfin(struct sqlite3 *db, int nbRun){

	char * debut = malloc(sizeof(char)*1024);
	char * fin = malloc(sizeof(char)*1024);
	int rc;
	pile *pileDate = NULL;
	char *zErrMsg = 0;

	pile *element = malloc(sizeof(pile));   
	if(!element) exit(EXIT_FAILURE);    
	element->valeur =  NULL;			// create a empty cell
	element->next =  NULL;


	char * commande = malloc(sizeof(char)*1024);
	sprintf(commande, "SELECT datedebut FROM info WHERE run = %d;",nbRun);


	rc = sqlite3_exec(db,commande ,callback_cal,&element, &zErrMsg);		//fill the linked list element with the query's result
	if( rc!=SQLITE_OK ){
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}

	pileDate = (pile *) element;  							// we cast element into pile * because the callback change it into a void *
	debut = pileDate->valeur;

	sprintf(commande, "SELECT datefin FROM info WHERE run = %d;",nbRun);	
	rc = sqlite3_exec(db,commande ,callback_cal,&element, &zErrMsg);		//fill the linked list element with the query's result
	if( rc!=SQLITE_OK ){
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}
	pileDate = (pile *) element;  							// we cast element into pile * because the callback change it into a void *

	fin = pileDate->valeur;

	printf("run : %d , debut : %s fin : %s \n",nbRun, debut, fin);
	supr(pileDate);

}

/***********************STATS : return the largest value of the list *****************************************************/

long long int plusgrand(pile * p){

	long long int resultat = 0;
	while(p->next){

		if(resultat <= atoi(p->valeur) )
			resultat = atoi(p->valeur);
		p = p->next;

	}

	return resultat;
}

/**************************STATS : Count the amout of drops of a thread in a run**************************************************/

long long int nbdrop(struct sqlite3 *db, int nbRun, char * thread){

	int rc = 0;
	pile *pileDrop = NULL;
	char *zErrMsg = 0;
	long long int resultat = 0;

	pile *element = malloc(sizeof(pile));
	if(!element) exit(EXIT_FAILURE);  
	element->valeur =  NULL;
	element->next =  NULL;				// Creat an empty cell
	pileDrop = element;       

	char * commande = malloc(sizeof(char)*1024);
	sprintf(commande, "SELECT col3  FROM param WHERE run = %d AND col1 = \"capture.kernel_drops\" %s;",nbRun,thread); 


	rc = sqlite3_exec(db,commande ,callback_cal,&element, &zErrMsg);	//fill the linked list element with the query's result
	if( rc!=SQLITE_OK ){
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}

	pileDrop = (pile *) element;

	resultat = plusgrand(pileDrop);

	supr(pileDrop);


	return resultat;
}

/*********************************STATS : Count the amout of packets of a thread in a run*******************************************/

long long int nbpacket(struct sqlite3 *db, int nbRun, char * thread){

	int rc = 0;
	pile *pilePacket = NULL;
	char *zErrMsg = 0;
	long long int resultat = 0;

	pile *element = malloc(sizeof(pile));
	if(!element) exit(EXIT_FAILURE);     
	element->valeur =  NULL;
	element->next =  NULL;
	pilePacket = element;

	char * commande = malloc(sizeof(char)*1024);
	sprintf(commande, "SELECT col3  FROM param WHERE run = %d AND col1 = \"capture.kernel_packets\" %s;",nbRun, thread);

	rc = sqlite3_exec(db,commande,callback_cal,&element, &zErrMsg);  //fill the linked list element with the query's result
	if( rc!=SQLITE_OK ){
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}
	pilePacket = (pile *) element;
	resultat = plusgrand(pilePacket);

	supr(pilePacket);

	return resultat;
}

/******************************STATS : calculates the ration of drops / packets**********************************************/

void calculRatioDrop(struct sqlite3 *db, int nbRun){

	long long int drops = nbdrop(db, nbRun, " ");
	long long int packets = nbpacket(db, nbRun," ");
	int resultat = 0;

	resultat = drops * 100 / packets;

	printf("ratio kernel drop pour run %d = %d / 100 \n",nbRun,resultat);
}

/************************STATS : Calculates the averange of drops packet of a thread in a run********************************************/

void moyennedroprun(struct sqlite3 * db, int nbRun, char * thread){

	long long int drops = 0;
	int duree = 0;	
	long long int resultat = 0;	

	char * commande = malloc(sizeof(char)*1024);
	sprintf(commande, "AND col2 = \"%s\"",thread);

	drops = nbdrop(db, nbRun,commande);
	duree = requeteDureeRun(db,nbRun);
	resultat = drops / duree;

	printf("durée du run %d = %d secondes \n", nbRun, duree);
	printf("run : %d, thread : %s, drop/sec = %lld \n",nbRun, thread, resultat);

}

/******************************STATS : Calculates the averange of packets of a thread in a run*************************************/

void moyennepacketrun(struct sqlite3 * db, int nbRun, char * thread){

	long long int packet = 0;
	int duree = 0;	
	long long int resultat = 0;
	char * commande = malloc(sizeof(char)*1024);

	sprintf(commande, "AND col2 = \"%s\" ",thread);
	packet = nbpacket(db, nbRun, commande);
	duree = requeteDureeRun(db,nbRun);
	resultat = packet / duree;

	printf("run : %d, thread : %s ,  packet/sec = %lld \n",nbRun,thread, resultat);

}

/******************************SUPPRESSION : delete the database**********************************************/

void supprim(char * filename){

	printf("suppr de %s\n ", filename);
	remove(filename);

}

/****************************************************************************/
/*************************************MAIN***********************************/
/****************************************************************************/
int main(int argc, char **argv){

	int opt; 
	struct sqlite3 *db = NULL;
	FILE * fichier;
	int nbRun, nbDump;
	fichier = ouverture("stat.log");

	while( (opt = getopt(argc, argv, "hcds")) !=-1){	// Switch between execution's options

		switch (opt){

			case 'h':
				if(argc ==  2){
					printf("--HELP-- \n -h --Aide \n -c [NomBase] --Creer une base de donnée \n -d [NomBase]--Supprimer une base de donnée \n -s [NomBase] [NbRun] [NomThread] --affiche les stats en fonction du run et du thread choisi\n");
					break;
				}else
					return -1;
			case 'c':
				if(argc == 3){
					db = createDataBase( db, argv[2]);
					printf("  En cours ...\n");
					createCommande(db, "PRAGMA synchronous = OFF;");	// Tell the database that it doesn't have to test every queries
					createCommande(db, "CREATE TABLE param (col1 char(50), col2 char(50), col3 int, dump int, run int);"); // create the database tables
					createCommande(db, "CREATE TABLE info (datedebut char(50),datefin char(50),heure int, uptime int, run int);");
					sscanf(parseDuFichier(fichier, db),"D:%d R:%d",&nbDump,&nbRun);
					printf("nb de run : %d, nb de dump %d\n", nbRun , nbDump);	
					break;
				}else
					return -1;
				
			case 'd':
				if(argc == 3){
					supprim( argv[2]);
					break;
				}else
					return -1;
			case 's':
				if(argc == 5){
					db = createDataBase( db, argv[2]);
					nbRun =  atoi(argv[3]);
					calculRatioDrop(db,nbRun);
					requetedebutfin(db,nbRun);
					moyennedroprun(db, nbRun, argv[4]);
					moyennepacketrun(db,nbRun, argv[4]);
					break;
				}else 
					return -1;
			default :
				exit(EXIT_FAILURE);		
		}
	}

	printf("  Fini!  \n");	
	return 0;
}
