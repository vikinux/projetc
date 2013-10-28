#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>


static int callback(void *NotUsed, int argc, char **argv, char **azColName){
	int i;
	if(NotUsed){}
	for(i=0; i<argc; i++){
		printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	}
	printf("\n");
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

	printf("%d  commande : %s\n\n",rc, commande);
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

void date( char * line){

	char * str = NULL;
	char * token = malloc(sizeof(char)*1024);
	char * token2 =  malloc(sizeof(char)*1024);

	
	token  = strtok_r( line, ":",&str);
	line = NULL;
	token2 = strtok_r(line, "-",&str);

	printf(":: %s :: %s \n",token,token2);

}

void param(char * line){

	char * token = malloc(sizeof(char)*1024);
	char * token2 =  malloc(sizeof(char)*1024);
	char * token3 =  malloc(sizeof(char)*1024);

	
	token  = strtok( line, " |\n");
	line = NULL;
	token2 = strtok(line, " |\n");
	line = NULL;
	token3 = strtok(line, " |\n");


	printf(":: %s :: %s :: %s\n",token,token2,token3);


}

void parseDuFichier( FILE * fichier){

	char * ligne = malloc(sizeof(char)*1024);




	while(fscanf(fichier, "%[^\n]\n", ligne) > 0){


	//	printf("%s \n",ligne);
		if(ligne[0] == '-'){
			fscanf(fichier, "%[^\n]\n", ligne); 
			date(ligne);
			fscanf(fichier, "%[^\n]\n", ligne) ;
			fscanf(fichier, "%[^\n]\n", ligne) ;
			fscanf(fichier, "%[^\n]\n", ligne) ;
		}
		else {
			param(ligne);
		}
	}

}

int main(int argc, char **argv){

	struct sqlite3 *db = NULL;
	FILE * fichier;

	fichier = ouverture("stat.log");

	parseDuFichier(fichier);	

	if(argc > 1) {
		db = createDataBase( db, argv[1]);
		createCommande(db, "CREATE TABLE bob (\"col1 char(50), col2 char(50)\");");
	}

	return 0;
}
