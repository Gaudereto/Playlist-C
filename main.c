
#include <stdio.h>
#include <stdlib.h>
#include <curl.h>
#include <string.h>
#include <jsmn.h>
#include <dirent.h>

// Navigation states:
enum states {
  START,
  CREATE,
  ADD,
  MANIPULATE
};
enum states state = START;

// Struct for spotify HTTP requests:
struct RequestStruct {
    char *url;
    char *method;
    char *body;
    char *header;
};

// Struct for spotify track info:
struct TrackInfo {
    char *title;
    char *artist;
    char *previewURL;
};

// Struct that will receive spotify response data:
struct MemoryStruct {
  char *memory;
  size_t size;
};

// Libcurl Callback function to store the received data in memory:
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(ptr == NULL) {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}
// Http networking function using libcurl:
char* letsNetwork(const char *url, const char *method, const char *body, const char *header){

  // Json message from http request:
  char *jMessage = NULL;
  // Curl operating variables:
  CURL *curl;
  CURLcode res;

  // Get a curl handle:
  curl = curl_easy_init();
  if(curl){
        // Initialize struct that will receive the data from spotify:
        struct MemoryStruct chunk;
        chunk.memory = malloc(1);  // will be grown as needed by the realloc in the callback function
        chunk.size = 0;            // no data at this point

        // Send all received data to: WriteMemoryCallback
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        // We pass the chunk struct to receive the data:
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        // HTTP header curl struct:
        struct curl_slist *httpHeader = NULL;
        httpHeader = curl_slist_append(httpHeader, header);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, httpHeader);

        // Define the destination url:
        curl_easy_setopt(curl, CURLOPT_URL, url);

        if(strcmp(method,"POST")==0){
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
        }

        // Do not verify SSL peer and host:
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);

        // HTTP communication:
        res = curl_easy_perform(curl);
        // Check communication:
        if(res != CURLE_OK){
            fprintf(stderr, "curl_easy_perform() failed: %s\n",
                  curl_easy_strerror(res));
        }

        // assign message to output pointer:
        jMessage = chunk.memory;
        //free(chunk.memory);
        curl_easy_cleanup(curl);
        curl_slist_free_all(httpHeader);
  }
  return jMessage;
}

// Function that compares a input string to a json token string:
int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
			strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

// JSON parser function that returns the user access token:
char* getAccessToken(char *js){

    int i;
    int r;
    char *accessToken = NULL;
    size_t len;
    jsmn_parser parser;

    // Jason Parsing routine
    // Firstly we get the number of necessary tokens for parsing the object:
    jsmn_init(&parser);
    r = jsmn_parse(&parser, js, strlen(js), NULL, 1);

    // Secondly we parse the object with the correct number of tokens:
    jsmntok_t tokens[r];
    jsmn_init(&parser);
    r = jsmn_parse(&parser, js, strlen(js), tokens, r);

    if (r < 0) {
		printf("Failed to parse JSON: %d\n", r);
	}
    // Loop over all keys of the root object:
    for (i = 1; i < r; i++) {
        // Find the access token key:
		if (jsoneq(js, &tokens[i], "access_token") == 0) {
			len = tokens[i+1].end-tokens[i+1].start;
            accessToken = calloc(len+1,sizeof(char));
            strncpy(accessToken,js + tokens[i+1].start,len);
            i++;
		}
    }
	return accessToken;
}

// JSON parser function that returns the searched track information:
struct TrackInfo* getTrackInfo(char *js){

    int i;
    int r;
    size_t len;
    struct TrackInfo *trackInformation = (struct TrackInfo *) calloc(1,sizeof(struct TrackInfo));
    jsmn_parser parser;
    jsmn_init(&parser);

    // Jason Parsing routine
    // Firstly we get the number of necessary tokens for parsing the object:
    jsmn_init(&parser);
    r = jsmn_parse(&parser, js, strlen(js), NULL, 1);

    // Secondly we parse the object with the correct number of tokens:
    jsmntok_t tokens[r];
    jsmn_init(&parser);
    r = jsmn_parse(&parser, js, strlen(js), tokens, r);

    if (r < 0) {
		printf("Failed to parse JSON: %d\n", r);
	}
    // Loop over all keys of the root object:
    for (i = 1; i < r; i++) {
        // Go over the object selecting all the desired information:
        if (jsoneq(js, &tokens[i], "album") == 0) {

			// Check if the last token was an object:
            if(tokens[i-1].type==1){
                // Finding the artist name:
                while(jsoneq(js, &tokens[i], "artists") != 0){
                    i++;
                }
                while(jsoneq(js, &tokens[i], "name") != 0){
                    i++;
                }
                len = tokens[i+1].end-tokens[i+1].start;
                trackInformation->artist = calloc(len+1,sizeof(char));
                strncpy(trackInformation->artist,js + tokens[i+1].start,len);
                // Finding the track title:
                while(jsoneq(js, &tokens[i], "is_playable") != 0){
                    i++;
                }
                i=i+2;
                len = tokens[i+1].end-tokens[i+1].start;
                trackInformation->title = calloc(len+1,sizeof(char));
                strncpy(trackInformation->title,js + tokens[i+1].start,len);
                // Finding the preview URL:
                while(jsoneq(js, &tokens[i], "preview_url") != 0){
                    i++;
                }
                len = tokens[i+1].end-tokens[i+1].start;
                trackInformation->previewURL = calloc(len+1,sizeof(char));
                strncpy(trackInformation->previewURL,js + tokens[i+1].start,len);
            }
		}
    }
	return trackInformation;
}

int main(void)
{
  // Application Header:
  printf("--------------------------------------------------------------------\n");
  printf("-----------------------------PLAYLIST-C-----------------------------\n");
  printf("--------------------------------------------------------------------\n");

  // Playlist variables:
  char playlistName[20];
  char playlistPath[40];
  FILE *myPlaylist;
  // Options char:
  char option;

  // First lets get the access token from SPOTIFY:
  // In windows, this will init the winsock stuff:
  curl_global_init(CURL_GLOBAL_ALL);

  // Request Access token message:
  struct RequestStruct tokenRequest;
  tokenRequest.url = "https://accounts.spotify.com/api/token";
  tokenRequest.method = "POST";
  tokenRequest.body = "grant_type=client_credentials";
  tokenRequest.header = "Authorization: Basic NzVjYWQ1ZWFlNGYwNDQ1NDg5MDcxMzIxZjU4MDM5MDE6ZTliMTJkMzU1MzUyNGE2ZmFhZWJjOWNmNDU2Mjg1ZWQ=";

  // Get the access token from spotify API:
  char *spotAnswer = letsNetwork(tokenRequest.url,tokenRequest.method,tokenRequest.body,tokenRequest.header);
  char *accessToken = getAccessToken(spotAnswer);
  free(spotAnswer);

  // STATE MACHINE FOR APP NAVIGATION:
  while(1){
    switch(state) {
    // Initial login/register state:
    case START:
        // Get User Name:
        printf("\nEnter a user name:\n");
        char userName[20];
        gets(userName);
        char usersFolder[26] = "users/";
        strcat(usersFolder,userName);
        // Opens users folder directory:
        DIR *dir = opendir(usersFolder);
        // Check if the user is already registered:
        if(dir!=NULL)
        {
            // Display user options:
            printf("\nOptions:\nc-> To create a new playlist:\na-> To access your saved playlists\n");
            scanf("%c",&option);
            fflush(stdin);
            if(option=='a'){state=MANIPULATE;}
            else if(option=='c'){state=CREATE;}
            else {printf("option invalid\n");}
        }
        else{
            // Create new directory for the new user:
            printf("User registered\n");
            mkdir(usersFolder);
            state=CREATE;
        }
        closedir(dir);
    break;
    // Playlist creation State:
    case CREATE:
        // Create new playlist:
        printf("\nEnter your new playlist name:\n");
        gets(playlistName);
        strcpy(playlistPath,usersFolder);
        strcat(playlistPath,"/");
        strcat(playlistPath,playlistName);
        strcat(playlistPath,".txt");
        myPlaylist = fopen(playlistPath,"w");
        fclose(myPlaylist);
        memset(playlistName,0,strlen(playlistName));
        state = ADD;
    break;
    // Playlist adding tracks state:
    case ADD:;
        // Open the playlist:
        myPlaylist = fopen(playlistPath,"a");
        // Track search message:
        // Building track search header:
        char *initialHeader = "Authorization: Bearer ";
        char *searchHeader = calloc(strlen(initialHeader)+strlen(accessToken)+1,sizeof(char));
        strcpy(searchHeader,initialHeader);
        strcat(searchHeader,accessToken);

        // Building track search URL:
        char *initialURL = "https://api.spotify.com/v1/search?q="; // Initial part of Track HTTP request
        char *endURL = "&type=track&market=BR&limit=1";            // Final part of Track HTTP request
        printf("\nEnter the track name you want to search:\n");
        char trackName[20];
        gets(trackName);
        int trackSize = strlen(trackName);
        int i;
        for(i=0;i<trackSize;i++){
            if(trackName[i]==' ')
            trackName[i]='+';
        }

        char *URL = calloc(strlen(initialURL)+strlen(trackName)+strlen(endURL)+1,sizeof(char));
        strcpy(URL,initialURL);
        strcat(URL,trackName);
        strcat(URL,endURL);

        // Search track struct:
        struct RequestStruct trackRequest;
        trackRequest.url = URL;
        trackRequest.method = "GET";
        trackRequest.body = "";
        trackRequest.header = searchHeader;

        // Get track from spotidycaralh
        spotAnswer = letsNetwork(trackRequest.url,trackRequest.method,trackRequest.body,trackRequest.header);
        struct TrackInfo *track = getTrackInfo(spotAnswer);
        if(track->title==NULL)
        {
            printf("Track not found\n");
            break;
        }
        printf("\nSearch Results:\nTitle: %s\nArtist: %s\n",track->title,track->artist);
        printf("\nOptions:\nc-> To confirm the track:\n");
        scanf("%c",&option);
        fflush(stdin);
        if(option=='c'){
                fputs(track->title,myPlaylist);
                fputs("\n",myPlaylist);
                fputs(track->artist,myPlaylist);
                fputs("\n",myPlaylist);
                fputs(track->previewURL,myPlaylist);
                fputs("\n",myPlaylist);
        }

        // Free memory and close files:
        fclose(myPlaylist);
        free(spotAnswer);
        free(track->artist);
        free(track->title);
        free(track->previewURL);
        free(track);
        free(searchHeader);
        free(URL);

        // User selects the next step:
        printf("\nOptions:\na-> To add a new song:\nm-> To go to your playlist menu\n");
        scanf("%c",&option);
        fflush(stdin);
        if(option=='a'){state=ADD;}
        else if(option=='m'){state=MANIPULATE;memset(playlistPath,0,strlen(playlistPath));}
        else {printf("option invalid\n");}
    break;

    case MANIPULATE:;
        // Open the user directory
        dir = opendir(usersFolder);
        struct dirent *pDirent;
        // Define a buffer to read the playlist file lines:
        char buff[255];
        // Display user's playlists:
        int counter = 1;
        printf("\nYour available playlists:\n");
        while ((pDirent = readdir(dir)) != NULL)
        {
            if( !strcmp(pDirent->d_name, ".") || !strcmp(pDirent->d_name, "..")){}
            else{
                    printf ("%d: %s\n", counter, pDirent->d_name);
                    counter++;}
        }
        // Close directory and reopen to get the desired file:
        closedir(dir);
        dir = opendir(usersFolder);
        // User selects the playlist:
        printf("\nOptions:\nnumber-> Enter the playlist number you want to access\n");
        scanf("%c",&option);
        fflush(stdin);
        int nOption = option - '0';
        // If the selected playlist is valid display its contents:
        if(nOption < counter){
            counter = 0;
            while ((pDirent = readdir(dir)) != NULL)
            {
                if( !strcmp(pDirent->d_name, ".") || !strcmp(pDirent->d_name, "..")){}
                else{counter++;}
                if(counter==nOption){
                    strcpy(playlistName,pDirent->d_name);
                    strcpy(playlistPath,usersFolder);
                    strcat(playlistPath,"/");
                    strcat(playlistPath,playlistName);
                    myPlaylist = fopen(playlistPath,"r");
                    char *status;
                    printf("\nDisplaying songs in the selected playlist:\n");
                    do{
                        status = fgets(buff,255,myPlaylist);
                        if(status==NULL){break;}
                        printf("Title: %s", buff );
                        status = fgets(buff,255,myPlaylist);
                        if(status==NULL){break;}
                        printf("Artist: %s",buff);
                        status = fgets(buff,255,myPlaylist);
                        if(status==NULL){break;}
                    }while(status);
                    fclose(myPlaylist);
                    memset(playlistName,0,strlen(playlistName));
                }
            }
        }
    // User selects the next step:
    printf("\nOptions:\nc-> To create a new playlist:\na-> To add a new song to the selected playlist\n");
    scanf("%c",&option);
    fflush(stdin);
    if(option=='c'){state=CREATE;}
    else if(option=='a'){state=ADD;}
    else {printf("option invalid\n");}
    break;
    }
  }

  curl_global_cleanup();
  return 0;
}





