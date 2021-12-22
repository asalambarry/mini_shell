/**
    Mini shell implementation
    @author
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>

#define COMMAND_LENGTH          1024 + 1
#define NB_COMMAND              16
#define OUTPUT_FILE_NOT_EXISTS  -1

/**
 * Créer un processus
 */
pid_t create_process(void) {

    pid_t pid;
    do {
        pid = fork();
    }
    while((pid == -1) && (errno == EAGAIN));

    return pid;
}

/**
 * Vider le buffer
 */ 
void viderBuffer(){
    int c = 0;
    while (c != '\n' && c != EOF) {
        c = getchar();
    }
}


/**
 * Teste si deux chaînes données sont égales
 *  @return int - 1 si oui ou 0 sinon
 */ 
int isEquals(char *t1, char *t2) {
    return ! strcmp(t1, t2);
}

/**
 * Analyser la ligne
 * Exercice 2
 */ 
int parse_line(char *s, char *argv[]) {
    const char* sep = " ";
    int j = 0;
    argv[j] = malloc(sizeof(char) * COMMAND_LENGTH);

    for (int i = 0; i < strlen(s); i++)
    {
        if(isspace(s[i])) {
            j++;
            argv[j] = malloc(sizeof(char) * COMMAND_LENGTH);
        }
        else {
            strncat(argv[j], &s[i], 1);
        } 
    }
    j++;
    argv[j] = NULL;
    return j;
}

/**
 * Check if there is a pipe at the end of the command
 */ 
int isPipe(char *argv[], int last) {

    if(last >= 2 && isEquals(argv[last-1], "|")) {
        return 1;
    }

    return 0;
}

/**
 * Execution de la ligne de commande
 */
int checkRedirectionPos(char *argv[], int last) {

    if(last < 3) {
        return OUTPUT_FILE_NOT_EXISTS;
    }

    if(isEquals(argv[last-2], ">")) {
        return last - 1;
    }

    return OUTPUT_FILE_NOT_EXISTS;
}

/**
 * Exécutez la commande avec "execv"
 */ 
void executeCommand(char *argv[]) {
    char *program = malloc(sizeof(char) * COMMAND_LENGTH);
    sprintf(program, "/bin/%s", argv[0]);

    // Try with /bin
    if(execv(program, argv) == -1) {

        // Try with /usr/bin
        sprintf(program, "/usr/bin/%s", argv[0]);
        if(execv(program, argv) == -1) {
            perror("execv");
        }
    }
}

// Utilité pour imprimer tous les arguments
void printArgs(char *argv[]) {
    int i;
    for (i = 0; argv[i] != NULL; i++)
    {
       printf("%s * ", argv[i]);
    }
    printf("NB=%d\n", i);
}

/**
 * Copier l'argument de src vers dest
 */ 
void copyArgsRemovingPipe(char *src[], char *dest[]) {
    int i;
    for (i = 0; src[i] != NULL, ! isEquals(src[i], "|"); i++)
    {
        dest[i] = malloc(sizeof(char) * COMMAND_LENGTH);
        strcpy(dest[i], src[i]);
    }
    dest[i] = NULL;
}

/**
 * Travail principal : Contrôleur
 */
void doWord(char *argv[], int nbArgs) {

     // Ouverture du fichier de redirection s'il y en a un : command ...args > output-file
    int filePos = checkRedirectionPos(argv, nbArgs);
    int oldfd, newfd;
    if(filePos != OUTPUT_FILE_NOT_EXISTS) {
        oldfd = open(argv[filePos], O_WRONLY | O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
        newfd = dup2(oldfd, STDOUT_FILENO);
        if(newfd == -1) {
            perror("dup2()");
        }
       // Couper les deux derniers arguments
        argv[filePos - 1] = NULL;
    }

    // printArgs(argv);

    // Exécuter la commande
    executeCommand(argv);

    // Fermeture du fichier de redirection s'il y en a un
    if(filePos != OUTPUT_FILE_NOT_EXISTS) {
        if(newfd != -1) {
            if(close(newfd) == -1) {
                perror("close");
            }

            if(close(oldfd) == -1) {
                perror("close");
            }   
        }
    }
}

/**
 * Lire une ligne de commande à partir de STDIN
 */ 
int readCommand(char *chaine, int longueur)
{
    char *positionEntree = NULL;
    if (fgets(chaine, longueur, stdin) != NULL) {

        positionEntree = strchr(chaine, '\n');
        if (positionEntree != NULL) {
            *positionEntree = '\0';
        }
        else {
            viderBuffer();
        }
        return 1;
    }
    else {
        viderBuffer();
        return 0;
    }
}

// Gérer la commande CTRL+C
void handler(int signo, siginfo_t *info, void *context){
}

void handler_child(int s) {
    raise(SIGTERM);
}

/**
 * Tuer le processus
 */ 
void term_handler_child(int signal) {
    kill(getpid(), SIGTERM);
}

int main() {

    // Enregistrer les signaux
    struct sigaction sig;               
    sigemptyset(&sig.sa_mask);      
    sig.sa_flags = SA_NODEFER;                
    sig.sa_sigaction = &handler;
    if(sigaction(SIGINT, &sig, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Créer du pipe
    int fildes[2];
    int status;

    status = pipe(fildes);
    if (status == -1 ) {
        perror("pipe");
    }

    pid_t pid;
    char command[COMMAND_LENGTH];
    char *argv[NB_COMMAND], 
        *tmpArgs[NB_COMMAND];
    int nbArgs, ispipe, secondCmd = 0;

    // La boucle while
    while(1) {

        printf("$ ");
        readCommand(command, COMMAND_LENGTH);

        if(isEquals(command, "exit")) {
            exit(EXIT_SUCCESS);
        }

        nbArgs = parse_line(command, argv);
        ispipe = isPipe(argv, nbArgs);

        // Fork
        pid = create_process();
        switch (pid) {

        case 0: 

            // Écoutez le signal de fin
            signal(SIGINT, handler_child);
            signal(SIGTERM, term_handler_child); 

            if(secondCmd) {
                close(fildes[1]);
                dup2(fildes[0], STDIN_FILENO);
                doWord(argv, nbArgs);
                close(fildes[0]);
            }
            else if( ! ispipe) {
                doWord(argv, nbArgs);
            }
            exit(EXIT_SUCCESS);
            break;

        default: // Le processus pere

            if(secondCmd) {
                close(fildes[0]);
                dup2(fildes[1], STDOUT_FILENO);
                executeCommand(tmpArgs);
                close(fildes[1]);
            } else if(ispipe) {
                copyArgsRemovingPipe(argv, tmpArgs);
            }

            if(wait(NULL) == -1) {
                perror("Wait");
                exit(EXIT_FAILURE);
            }
            break;
        }

        secondCmd = (ispipe && ! secondCmd) ? 1 : 0;
    }

    return EXIT_SUCCESS;
}
