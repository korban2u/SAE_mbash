#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <glob.h>
#include <signal.h>
#include <termios.h>
#include <time.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h> // Pour obtenir la taille du terminal

// Codes de couleur ANSI améliorés
#define ANSI_COLOR_BLACK   "\x1b[30m"
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_WHITE   "\x1b[37m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_BOLD          "\x1b[1m"
#define ANSI_ITALIC        "\x1b[3m"
#define ANSI_UNDERLINE     "\x1b[4m"
#define ANSI_BLINK         "\x1b[5m"
#define ANSI_REVERSE       "\x1b[7m"
#define ANSI_HIDDEN        "\x1b[8m"

// Couleurs de fond
#define BG_BLACK           "\x1b[40m"
#define BG_RED             "\x1b[41m"
#define BG_GREEN           "\x1b[42m"
#define BG_YELLOW          "\x1b[43m"
#define BG_BLUE            "\x1b[44m"
#define BG_MAGENTA         "\x1b[45m"
#define BG_CYAN            "\x1b[46m"
#define BG_WHITE           "\x1b[47m"

// Symboles Unicode pour des indicateurs visuels
#define ARROW_RIGHT        "➜"
#define ARROW_LEFT         "←"
#define CHECK_MARK         "✓"
#define CROSS_MARK         "✗"
#define FOLDER_ICON        "📁"
#define FILE_ICON          "📄"
#define LOCK_ICON          "🔒"
#define TIME_ICON          "⏱️"
#define POWER_ICON         "⚡"
#define HOME_ICON          "🏠"
#define GIT_BRANCH         "⎇"
#define WARNING_ICON       "⚠️"
#define ERROR_ICON         "❌"
#define INFO_ICON          "ℹ️"

// Codes de contrôle du terminal
#define KEY_ESC  27
#define KEY_TAB  9
#define KEY_BACKSPACE 127

// Taille maximale pour une ligne de commande
#define TAILLE_MAX_COMMANDE 1024
#define TAILLE_MAX_HISTORIQUE 100
#define TAILLE_MAX_COMPLETION 256
#define TAILLE_MAX_BUFFER_LIGNE 2048

// Structure pour l'historique des commandes
typedef struct {
    char commandes[TAILLE_MAX_HISTORIQUE][TAILLE_MAX_COMMANDE];
    int debut;
    int fin;
    int actuel;
} Historique;

// Structure pour le buffer de ligne d'édition
typedef struct {
    char buffer[TAILLE_MAX_BUFFER_LIGNE];
    int position;        // Position actuelle du curseur
    int longueur;        // Longueur actuelle du buffer
    int historique_pos;  // Position dans l'historique
} Buffer_ligne;

// Déclaration de la variable globale environ
extern char **environ;

// Variables globales
Historique historique = {.debut = 0, .fin = 0, .actuel = 0};
struct termios term_original;
int mode_commande = 1;  // Mode par défaut: 1 pour interactif, 0 pour script
char completion_matches[TAILLE_MAX_COMPLETION][TAILLE_MAX_COMMANDE];
int nb_completion_matches = 0;
int dernier_code_retour = 0;  // Pour stocker le code de retour de la dernière commande

// Prototypes des fonctions
void changer_repertoire(char *chemin);
void afficher_repertoire_courant();
void executer_commande(char **arguments, int en_arriere_plan);
void analyser_et_executer_ligne(char *ligne);
char* remplacer_variable(char *commande);
void remplacer_motifs(char *commande);
char *chercher_commande_dans_path(char *commande);
void gestionnaire_sigchld(int sig);
void afficher_aide();
void initialiser_terminal();
void restaurer_terminal();
void ajouter_historique(const char *commande);
void afficher_historique();
void charger_rc();
char *construire_prompt();
void afficher_banniere();
void afficher_erreur(const char *message, const char *detail);
void afficher_info(const char *message);
void afficher_avertissement(const char *message);
void afficher_succes(const char *message);

// Prototypes des fonctions pour l'édition de ligne et la complétion
void initialiser_buffer_ligne(Buffer_ligne *bl);
char *lire_ligne_editee();
void afficher_buffer(Buffer_ligne *bl, int curseur_seulement);
void insérer_caractere(Buffer_ligne *bl, char c);
void supprimer_caractere(Buffer_ligne *bl);
void deplacer_curseur_gauche(Buffer_ligne *bl);
void deplacer_curseur_droite(Buffer_ligne *bl);
int lire_touche();
void trouver_completions(const char *prefixe);
int completer_commande(Buffer_ligne *bl);
int verifier_executable(const char *chemin);
void lister_repertoire(const char *prefixe, const char *repertoire);
void lister_commandes_dans_path(const char *prefixe);
char *trouver_dernier_mot(const char *chaine, int position);

// Fonction pour afficher un message d'erreur avec style
void afficher_erreur(const char *message, const char *detail) {
    fprintf(stderr, "\n  " ERROR_ICON " " ANSI_COLOR_RED ANSI_BOLD "Erreur:" ANSI_COLOR_RESET " %s", message);
    if (detail != NULL) {
        fprintf(stderr, " (%s)", detail);
    }
    fprintf(stderr, "\n\n");
}

// Fonction pour afficher un message d'information avec style
void afficher_info(const char *message) {
    printf("\n  " INFO_ICON " " ANSI_COLOR_CYAN "%s" ANSI_COLOR_RESET "\n\n", message);
}

// Fonction pour afficher un message d'avertissement avec style
void afficher_avertissement(const char *message) {
    printf("\n  " WARNING_ICON " " ANSI_COLOR_YELLOW "%s" ANSI_COLOR_RESET "\n\n", message);
}

// Fonction pour afficher un message de succès avec style
void afficher_succes(const char *message) {
    printf("\n  " CHECK_MARK " " ANSI_COLOR_GREEN "%s" ANSI_COLOR_RESET "\n\n", message);
}

// Fonction pour afficher une bannière moderne au démarrage
void afficher_banniere() {
    printf("\n");
    // Couleurs gradient moderne
    printf("\x1b[38;2;41;128;185m"); // Bleu clair
    printf("  ┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\n");
    printf("  ┃                                                 ┃\n");
    printf("\x1b[38;2;52;152;219m"); // Transition
    printf("  ┃  ███╗   ███╗██████╗  █████╗ ███████╗██╗  ██╗   ┃\n");
    printf("  ┃  ████╗ ████║██╔══██╗██╔══██╗██╔════╝██║  ██║   ┃\n");
    printf("\x1b[38;2;41;128;185m"); // Bleu moyen
    printf("  ┃  ██╔████╔██║██████╔╝███████║███████╗███████║   ┃\n");
    printf("  ┃  ██║╚██╔╝██║██╔══██╗██╔══██║╚════██║██╔══██║   ┃\n");
    printf("\x1b[38;2;26;188;156m"); // Vert-bleu
    printf("  ┃  ██║ ╚═╝ ██║██████╔╝██║  ██║███████║██║  ██║   ┃\n");
    printf("  ┃  ╚═╝     ╚═╝╚═════╝ ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝   ┃\n");
    printf("  ┃                                                 ┃\n");
    printf("\x1b[38;2;22;160;133m"); // Vert foncé
    printf("  ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\n");
    printf(ANSI_COLOR_RESET);
    
    time_t temps_actuel;
    time(&temps_actuel);
    struct tm *temps_local = localtime(&temps_actuel);
    char date_heure[128];
    strftime(date_heure, sizeof(date_heure), "%A %d %B %Y, %H:%M:%S", temps_local);
    
    printf("  %s\n", date_heure);
    printf("  " ANSI_COLOR_CYAN "%s " ANSI_ITALIC "Mini Shell" ANSI_COLOR_RESET " - v1.0\n", POWER_ICON);
    printf("  " ANSI_COLOR_YELLOW "Par Ryan Korban Vivein Herman\n" ANSI_COLOR_RESET);
    printf("\n");
    printf("  Tapez " BG_BLUE ANSI_COLOR_WHITE " help " ANSI_COLOR_RESET " pour afficher l'aide\n");
    printf("\n");
}

// Fonction pour construire un prompt personnalisé moderne
char *construire_prompt() {
    static char prompt[TAILLE_MAX_COMMANDE];
    char hostname[256];
    char cwd[TAILLE_MAX_COMMANDE];
    char *username = getenv("USER");
    
    if (!username) username = "user";
    if (gethostname(hostname, sizeof(hostname)) != 0) strcpy(hostname, "localhost");
    if (getcwd(cwd, sizeof(cwd)) == NULL) strcpy(cwd, "???");
    
    // Informations système pour le prompt
    char heure_actuelle[10];
    time_t temps_actuel;
    time(&temps_actuel);
    struct tm *temps_local = localtime(&temps_actuel);
    strftime(heure_actuelle, sizeof(heure_actuelle), "%H:%M", temps_local);
    
    // Détecter si nous sommes en root
    int est_root = (geteuid() == 0);
    
    // Format moderne: [heure] user@host:~/path $ (avec indicateurs d'état)
    char *home = getenv("HOME");
    char chemin_affichage[TAILLE_MAX_COMMANDE];
    
    if (home && strncmp(cwd, home, strlen(home)) == 0) {
        // Remplacer le chemin HOME par ~
        sprintf(chemin_affichage, "~%s", cwd + strlen(home));
    } else {
        strcpy(chemin_affichage, cwd);
    }
    
    // Construction du prompt avec style moderne mais SANS le \n initial
    sprintf(prompt, 
            "" // Suppression du \n ici
            "\x1b[38;2;189;195;199m%s " // Gris clair pour l'horloge
            "%s "                // Statut de commande
            "\x1b[38;2;52;152;219m%s" ANSI_BOLD "@%s" ANSI_COLOR_RESET // Nom utilisateur@hôte
            "\x1b[38;2;189;195;199m:" // Séparateur
            "\x1b[38;2;46;204;113m%s%s" ANSI_COLOR_RESET // Chemin répertoire
            " %s ", // Indicateur final
            
            heure_actuelle,
            dernier_code_retour == 0 ? 
                "\x1b[38;2;46;204;113m" CHECK_MARK ANSI_COLOR_RESET : 
                "\x1b[38;2;231;76;60m" CROSS_MARK ANSI_COLOR_RESET,
            username, hostname,
            est_root ? LOCK_ICON " " : FOLDER_ICON " ", chemin_affichage,
            est_root ? 
                "\x1b[38;2;231;76;60m#" ANSI_COLOR_RESET : 
                "\x1b[38;2;52;152;219m" ARROW_RIGHT ANSI_COLOR_RESET
    );
    
    return prompt;
}

// Fonction pour ajouter une commande à l'historique
void ajouter_historique(const char *commande) {
    if (strlen(commande) > 0 && commande[0] != ' ') {  // Ignorer les commandes vides ou commençant par un espace
        strcpy(historique.commandes[historique.fin], commande);
        historique.fin = (historique.fin + 1) % TAILLE_MAX_HISTORIQUE;
        
        // Si l'historique est plein, déplacer le début
        if (historique.fin == historique.debut) {
            historique.debut = (historique.debut + 1) % TAILLE_MAX_HISTORIQUE;
        }
        
        historique.actuel = historique.fin;  // Mettre à jour la position actuelle
    }
}

// Fonction pour afficher l'historique des commandes avec style moderne
void afficher_historique() {
    int i = historique.debut;
    int num = 1;
    
    printf("\n");
    printf("  " BG_BLUE ANSI_COLOR_WHITE ANSI_BOLD " Historique " ANSI_COLOR_RESET "\n\n");
    
    if (historique.debut == historique.fin) {
        printf("  " INFO_ICON " " ANSI_ITALIC "L'historique est vide" ANSI_COLOR_RESET "\n\n");
        return;
    }
    
    printf("  ┌───────┬─────────────────────────────────────────────────────┐\n");
    while (i != historique.fin) {
        printf("  │ " ANSI_COLOR_YELLOW "%3d" ANSI_COLOR_RESET " │ %-53s │\n", num, historique.commandes[i]);
        i = (i + 1) % TAILLE_MAX_HISTORIQUE;
        num++;
        
        // Ajouter une ligne de séparation entre chaque entrée
        if (i != historique.fin) {
            printf("  ├───────┼─────────────────────────────────────────────────────┤\n");
        }
    }
    printf("  └───────┴─────────────────────────────────────────────────────┘\n\n");
}

// Fonction pour initialiser le terminal en mode non-canonique
void initialiser_terminal() {
    struct termios term;
    
    // Sauvegarder les paramètres originaux
    tcgetattr(STDIN_FILENO, &term_original);
    
    // Copier les paramètres et les modifier
    term = term_original;
    term.c_lflag &= ~(ICANON | ECHO);  // Désactiver le mode canonique et l'écho
    term.c_cc[VMIN] = 1;               // Lire caractère par caractère
    term.c_cc[VTIME] = 0;              // Pas de timeout
    
    // Appliquer les nouveaux paramètres
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
}

// Fonction pour restaurer les paramètres originaux du terminal
void restaurer_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &term_original);
}

// Fonction pour charger le fichier de configuration .mbashrc
void charger_rc() {
    char *home = getenv("HOME");
    if (home == NULL) return;
    
    char rc_path[TAILLE_MAX_COMMANDE];
    snprintf(rc_path, TAILLE_MAX_COMMANDE, "%s/.mbashrc", home);
    
    FILE *rc_file = fopen(rc_path, "r");
    if (rc_file == NULL) return;
    
    char ligne[TAILLE_MAX_COMMANDE];
    while (fgets(ligne, TAILLE_MAX_COMMANDE, rc_file) != NULL) {
        ligne[strcspn(ligne, "\n")] = '\0';  // Supprimer le retour à la ligne
        analyser_et_executer_ligne(ligne);
    }
    
    fclose(rc_file);
}

// Fonction pour initialiser le buffer de ligne
void initialiser_buffer_ligne(Buffer_ligne *bl) {
    memset(bl->buffer, 0, TAILLE_MAX_BUFFER_LIGNE);
    bl->position = 0;
    bl->longueur = 0;
    bl->historique_pos = -1;
}

// Fonction pour afficher le buffer de ligne (avec positionnement du curseur)
void afficher_buffer(Buffer_ligne *bl, int curseur_seulement) {
    // Retourner au début de la ligne et effacer tout ce qui s'y trouve
    printf("\r\033[K");
    
    // Afficher le prompt (sans le \n supplémentaire)
    // Obtenir le prompt mais supprimer le premier caractère \n qui cause le problème
    char *prompt_complet = construire_prompt();
    char *prompt = prompt_complet;
    if (prompt[0] == '\n') {
        prompt++; // Sauter le \n initial si présent
    }
    
    // Afficher le prompt et le contenu du buffer
    printf("%s", prompt);
    
    if (!curseur_seulement) {
        // Afficher tout le buffer
        printf("%s", bl->buffer);
    } else {
        // Positionner le curseur à la bonne position
        for (int i = 0; i < bl->position; i++) {
            printf("%c", bl->buffer[i]);
        }
    }
    
    // S'assurer que tout est affiché immédiatement
    fflush(stdout);
}

// Fonction pour insérer un caractère dans le buffer
void insérer_caractere(Buffer_ligne *bl, char c) {
    if (bl->longueur >= TAILLE_MAX_BUFFER_LIGNE - 1) return;
    
    // Décaler les caractères à droite
    for (int i = bl->longueur; i > bl->position; i--) {
        bl->buffer[i] = bl->buffer[i - 1];
    }
    
    // Insérer le nouveau caractère
    bl->buffer[bl->position] = c;
    bl->position++;
    bl->longueur++;
    bl->buffer[bl->longueur] = '\0';
    
    // Réafficher le buffer
    afficher_buffer(bl, 0);
}


// Fonction pour supprimer un caractère à la position actuelle (backspace)
void supprimer_caractere(Buffer_ligne *bl) {
    if (bl->position <= 0) return;
    
    // Décaler les caractères à gauche
    for (int i = bl->position - 1; i < bl->longueur; i++) {
        bl->buffer[i] = bl->buffer[i + 1];
    }
    
    bl->position--;
    bl->longueur--;
    bl->buffer[bl->longueur] = '\0';
    
    // Réafficher le buffer
    afficher_buffer(bl, 0);
}

// Fonction pour déplacer le curseur vers la gauche
void deplacer_curseur_gauche(Buffer_ligne *bl) {
    if (bl->position > 0) {
        bl->position--;
        afficher_buffer(bl, 1);
    }
}

// Fonction pour déplacer le curseur vers la droite
void deplacer_curseur_droite(Buffer_ligne *bl) {
    if (bl->position < bl->longueur) {
        bl->position++;
        afficher_buffer(bl, 1);
    }
}

// Fonction pour lire une touche
int lire_touche() {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return -1;
    
    if (c == KEY_ESC) {
        // Séquence d'échappement (touches spéciales)
        char seq[3];
        
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return KEY_ESC;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_ESC;
        
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return 1000;  // Flèche haut
                case 'B': return 1001;  // Flèche bas
                case 'C': return 1002;  // Flèche droite
                case 'D': return 1003;  // Flèche gauche
                case 'H': return 1004;  // Touche Home
                case 'F': return 1005;  // Touche End
            }
        }
        
        return KEY_ESC;
    } else {
        return c;
    }
}

// Fonction pour vérifier si un fichier est exécutable
int verifier_executable(const char *chemin) {
    struct stat st;
    
    if (stat(chemin, &st) == 0) {
        if (S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR)) {
            return 1;  // Fichier régulier et exécutable
        }
    }
    
    return 0;
}

// Fonction pour trouver le dernier mot dans une chaîne
char *trouver_dernier_mot(const char *chaine, int position) {
    static char mot[TAILLE_MAX_COMMANDE];
    int debut = position;
    
    // Reculer jusqu'au début du mot ou au début de la chaîne
    while (debut > 0 && !isspace(chaine[debut - 1])) {
        debut--;
    }
    
    // Copier le mot
    strncpy(mot, chaine + debut, position - debut);
    mot[position - debut] = '\0';
    
    return mot;
}

// Fonction pour lister les fichiers et répertoires correspondant au préfixe
void lister_repertoire(const char *prefixe, const char *repertoire) {
    DIR *dir;
    struct dirent *entry;
    int prefixe_len = strlen(prefixe);
    char chemin_complet[TAILLE_MAX_COMMANDE];
    
    if ((dir = opendir(repertoire)) == NULL) {
        return;
    }
    
    while ((entry = readdir(dir)) != NULL && nb_completion_matches < TAILLE_MAX_COMPLETION) {
        // Ignorer . et ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Vérifier si le nom correspond au préfixe
        if (strncmp(entry->d_name, prefixe, prefixe_len) == 0) {
            // Construire le chemin complet
            snprintf(chemin_complet, TAILLE_MAX_COMMANDE, "%s/%s", 
                     (strcmp(repertoire, ".") == 0) ? "" : repertoire, entry->d_name);
            
            // Vérifier si c'est un répertoire pour ajouter un /
            struct stat st;
            if (stat(chemin_complet, &st) == 0 && S_ISDIR(st.st_mode)) {
                snprintf(completion_matches[nb_completion_matches], TAILLE_MAX_COMMANDE, 
                         "%s/", entry->d_name);
            } else {
                strcpy(completion_matches[nb_completion_matches], entry->d_name);
            }
            
            nb_completion_matches++;
        }
    }
    
    closedir(dir);
}

// Fonction pour lister les commandes dans le PATH
void lister_commandes_dans_path(const char *prefixe) {
    char *path_env = getenv("PATH");
    if (path_env == NULL) return;
    
    char *path_copy = strdup(path_env);
    char *repertoire = strtok(path_copy, ":");
    int prefixe_len = strlen(prefixe);
    
    while (repertoire != NULL && nb_completion_matches < TAILLE_MAX_COMPLETION) {
        DIR *dir = opendir(repertoire);
        if (dir != NULL) {
            struct dirent *entry;
            
            while ((entry = readdir(dir)) != NULL && nb_completion_matches < TAILLE_MAX_COMPLETION) {
                // Vérifier si le fichier correspond au préfixe et est exécutable
                if (strncmp(entry->d_name, prefixe, prefixe_len) == 0) {
                    char chemin_complet[TAILLE_MAX_COMMANDE];
                    snprintf(chemin_complet, TAILLE_MAX_COMMANDE, "%s/%s", repertoire, entry->d_name);
                    
                    if (verifier_executable(chemin_complet)) {
                        strcpy(completion_matches[nb_completion_matches], entry->d_name);
                        nb_completion_matches++;
                    }
                }
            }
            
            closedir(dir);
        }
        
        repertoire = strtok(NULL, ":");
    }
    
    free(path_copy);
}

// Fonction pour trouver les complétions possibles
void trouver_completions(const char *prefixe) {
    nb_completion_matches = 0;
    
    // Si le préfixe contient un / ou commence par ., c'est un chemin
    if (strchr(prefixe, '/') != NULL || prefixe[0] == '.') {
        // Extraire le répertoire et le préfixe de fichier
        char repertoire[TAILLE_MAX_COMMANDE];
        char fichier_prefixe[TAILLE_MAX_COMMANDE];
        char *dernier_slash = strrchr(prefixe, '/');
        
        if (dernier_slash) {
            // Préfixe avec chemin (ex: /usr/b ou ./s)
            strncpy(repertoire, prefixe, dernier_slash - prefixe);
            repertoire[dernier_slash - prefixe] = '\0';
            strcpy(fichier_prefixe, dernier_slash + 1);
            
            // Si le répertoire est vide, utiliser /
            if (repertoire[0] == '\0') {
                strcpy(repertoire, "/");
            }
        } else {
            // Préfixe relatif (ex: ./file)
            strcpy(repertoire, ".");
            strcpy(fichier_prefixe, prefixe);
        }
        
        lister_repertoire(fichier_prefixe, repertoire);
    } else {
        // Complétion de commande
        lister_commandes_dans_path(prefixe);
        
        // Si nous sommes au début de la ligne, chercher aussi dans le répertoire courant
        lister_repertoire(prefixe, ".");
    }
}

// Fonction pour compléter la commande avec un style moderne
int completer_commande(Buffer_ligne *bl) {
    // Trouver le mot à compléter
    char *mot = trouver_dernier_mot(bl->buffer, bl->position);
    
    // Aucun mot à compléter
    if (strlen(mot) == 0) return 0;
    
    // Trouver les complétions
    trouver_completions(mot);
    
    if (nb_completion_matches == 0) {
        // Aucune complétion trouvée
        return 0;
    } else if (nb_completion_matches == 1) {
        // Une seule complétion, l'appliquer directement
        // Supprimer le mot à compléter
        int mot_len = strlen(mot);
        for (int i = 0; i < mot_len; i++) {
            supprimer_caractere(bl);
        }
        
        // Insérer la complétion
        for (int i = 0; i < strlen(completion_matches[0]); i++) {
            insérer_caractere(bl, completion_matches[0][i]);
        }
        
        return 1;
    } else {
        // Plusieurs complétions, afficher les possibilités avec un style moderne
        printf("\n");
        
        // Déterminer la longueur maximale pour un affichage harmonieux
        int max_len = 0;
        for (int i = 0; i < nb_completion_matches; i++) {
            int len = strlen(completion_matches[i]);
            if (len > max_len) max_len = len;
        }
        
        // Calculer le nombre de colonnes selon la largeur du terminal
        struct winsize ws;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
        int col_width = max_len + 4; // Espacement entre colonnes
        int nb_cols = ws.ws_col / col_width;
        if (nb_cols < 1) nb_cols = 1;
        
        // Afficher les complétions en lignes/colonnes
        printf("  ");
        for (int i = 0; i < nb_completion_matches; i++) {
            // Choisir une couleur selon le type (répertoire, exécutable, etc.)
            char *match = completion_matches[i];
            int is_dir = (match[strlen(match) - 1] == '/');
            
            if (is_dir) {
                printf(ANSI_COLOR_BLUE ANSI_BOLD "%-*s" ANSI_COLOR_RESET, col_width, match);
            } else if (verifier_executable(match)) {
                printf(ANSI_COLOR_GREEN "%-*s" ANSI_COLOR_RESET, col_width, match);
            } else {
                printf("%-*s", col_width, match);
            }
            
            if ((i + 1) % nb_cols == 0 && i < nb_completion_matches - 1) {
                printf("\n  ");
            }
        }
        printf("\n");
        
        // Réafficher le prompt et le buffer
        afficher_buffer(bl, 0);
        
        return 1;
    }
}

// Fonction pour lire une ligne avec édition et complétion
char *lire_ligne_editee() {
    static char ligne[TAILLE_MAX_COMMANDE];
    Buffer_ligne bl;
    initialiser_buffer_ligne(&bl);
    
    // Afficher le buffer initial (le prompt)
    afficher_buffer(&bl, 0);
    
    while (1) {
        int c = lire_touche();
        
        if (c == '\n' || c == '\r') {
            // Fin de ligne (Enter)
            printf("\n");
            strcpy(ligne, bl.buffer);
            return ligne;
        } else if (c == KEY_BACKSPACE) {
            // Backspace
            supprimer_caractere(&bl);
        } else if (c == KEY_TAB) {
            // Tab (complétion)
            completer_commande(&bl);
        } else if (c == 1002) {
            // Flèche droite
            deplacer_curseur_droite(&bl);
        } else if (c == 1003) {
            // Flèche gauche
            deplacer_curseur_gauche(&bl);
        } else if (c == 1000) {
            // Flèche haut (historique précédent)
            if (bl.historique_pos < historique.fin - historique.debut - 1) {
                bl.historique_pos++;
                int index = (historique.fin - 1 - bl.historique_pos) % TAILLE_MAX_HISTORIQUE;
                if (index < 0) index += TAILLE_MAX_HISTORIQUE;
                strcpy(bl.buffer, historique.commandes[index]);
                bl.position = bl.longueur = strlen(bl.buffer);
                afficher_buffer(&bl, 0);
            }
        } else if (c == 1001) {
            // Flèche bas (historique suivant)
            if (bl.historique_pos > 0) {
                bl.historique_pos--;
                int index = (historique.fin - 1 - bl.historique_pos) % TAILLE_MAX_HISTORIQUE;
                if (index < 0) index += TAILLE_MAX_HISTORIQUE;
                strcpy(bl.buffer, historique.commandes[index]);
                bl.position = bl.longueur = strlen(bl.buffer);
                afficher_buffer(&bl, 0);
            } else if (bl.historique_pos == 0) {
                // Revenir à une ligne vide
                bl.historique_pos = -1;
                bl.buffer[0] = '\0';
                bl.position = bl.longueur = 0;
                afficher_buffer(&bl, 0);
            }
        } else if (c == 1004) {
            // Home (début de ligne)
            bl.position = 0;
            afficher_buffer(&bl, 1);
        } else if (c == 1005) {
            // End (fin de ligne)
            bl.position = bl.longueur;
            afficher_buffer(&bl, 1);
        } else if (c == 4) {
            // Ctrl+D (EOF)
            if (bl.longueur == 0) {
                printf("\n");
                return NULL;  // Indiquer la fin (comme EOF)
            }
        } else if (c == 3) {
            // Ctrl+C (interruption)
            printf("\n");
            bl.buffer[0] = '\0';
            return bl.buffer;  // Retourner une ligne vide
        } else if (c >= 32 && c < 127) {
            // Caractère imprimable
            insérer_caractere(&bl, c);
        }
    }
}

// Fonction pour remplacer une variable d'environnement par sa valeur
char* remplacer_variable(char *commande) {
    static char resultat[TAILLE_MAX_COMMANDE];
    
    if (commande[0] == '$') {
        char *variable = getenv(commande + 1); // Récupérer la variable d'environnement sans le '$'
        if (variable != NULL) {
            return variable; // Retourner la valeur de la variable
        } else {
            // Variable non trouvée, retourner une chaîne vide
            return "";
        }
    }
    
    // Si c'est une chaîne complexe avec des variables à l'intérieur
    if (strchr(commande, '$') != NULL) {
        strcpy(resultat, commande);
        char *debut = resultat;
        char *p = strchr(debut, '$');
        
        while (p != NULL) {
            char nom_var[TAILLE_MAX_COMMANDE] = "";
            char *q = p + 1;
            int i = 0;
            
            // Extraire le nom de la variable
            while (*q && ((*q >= 'A' && *q <= 'Z') || (*q >= 'a' && *q <= 'z') || 
                         (*q >= '0' && *q <= '9') || *q == '_')) {
                nom_var[i++] = *q++;
            }
            nom_var[i] = '\0';
            
            // Chercher la valeur de la variable
            char *valeur = getenv(nom_var);
            if (valeur != NULL) {
                // Remplacer la variable par sa valeur
                char avant[TAILLE_MAX_COMMANDE] = "";
                char apres[TAILLE_MAX_COMMANDE] = "";
                
                strncpy(avant, resultat, p - resultat);
                avant[p - resultat] = '\0';
                strcpy(apres, q);
                
                sprintf(resultat, "%s%s%s", avant, valeur, apres);
                debut = resultat + (p - resultat) + strlen(valeur);
            } else {
                // Variable non trouvée, la laisser telle quelle
                debut = q;
            }
            
            p = strchr(debut, '$');
        }
        
        return resultat;
    }
    
    return commande; // Si ce n'est pas une variable, renvoyer la commande originale
}

// Fonction pour remplacer les caractères spéciaux (comme * ou ?) par les correspondances de fichiers
void remplacer_motifs(char *commande) {
    glob_t glob_res;
    if (glob(commande, 0, NULL, &glob_res) == 0) {
        if (glob_res.gl_pathc > 0) {
            strcpy(commande, glob_res.gl_pathv[0]);
        }
    }
    globfree(&glob_res);
}

// Fonction pour chercher la commande dans les répertoires de PATH
char *chercher_commande_dans_path(char *commande) {
    // Si la commande contient un '/', c'est un chemin relatif ou absolu
    if (strchr(commande, '/') != NULL) {
        if (access(commande, X_OK) == 0) {
            return strdup(commande);
        }
        return NULL;
    }
    
    char *path_env = getenv("PATH");
    if (path_env == NULL) {
        afficher_erreur("La variable d'environnement PATH est absente.", NULL);
        return NULL;
    }

    char *path_copy = strdup(path_env); // Copie de PATH pour le découper
    char *repertoire = strtok(path_copy, ":");

    while (repertoire != NULL) {
        // Concaténer le répertoire avec la commande
        char *chemin = malloc(TAILLE_MAX_COMMANDE);
        snprintf(chemin, TAILLE_MAX_COMMANDE, "%s/%s", repertoire, commande);

        // Vérifier si le fichier existe et est exécutable
        if (access(chemin, X_OK) == 0) {
            free(path_copy);
            return chemin; // Retourner le chemin complet de la commande
        }

        free(chemin);
        repertoire = strtok(NULL, ":");
    }

    free(path_copy);
    return NULL; // Commande non trouvée
}

// Fonction pour exécuter une commande avec execve
void executer_commande(char **arguments, int en_arriere_plan) {
    if (arguments[0] == NULL) return;
    
    // Vérifier si c'est une commande intégrée
    if (strcmp(arguments[0], "cd") == 0) {
        if (arguments[1] != NULL) {
            changer_repertoire(arguments[1]);
        } else {
            // cd sans argument = cd $HOME
            changer_repertoire(getenv("HOME"));
        }
        dernier_code_retour = 0;
        return;
    } else if (strcmp(arguments[0], "pwd") == 0) {
        afficher_repertoire_courant();
        dernier_code_retour = 0;
        return;
    } else if (strcmp(arguments[0], "exit") == 0) {
        afficher_info("Au revoir !");
        exit(0);
    } else if (strcmp(arguments[0], "echo") == 0) {
        for (int i = 1; arguments[i] != NULL; i++) {
            char *valeur = remplacer_variable(arguments[i]);
            printf("%s ", valeur);
        }
        printf("\n");
        dernier_code_retour = 0;
        return;
    } else if (strcmp(arguments[0], "set") == 0) {
        if (arguments[1] != NULL && arguments[2] != NULL) {
            setenv(arguments[1], arguments[2], 1);
            afficher_succes("Variable définie avec succès");
            dernier_code_retour = 0;
        } else {
            afficher_erreur("Usage: set NOM_VARIABLE valeur", NULL);
            dernier_code_retour = 1;
        }
        return;
    } else if (strcmp(arguments[0], "history") == 0) {
        afficher_historique();
        dernier_code_retour = 0;
        return;
    } else if (strcmp(arguments[0], "help") == 0) {
        afficher_aide();
        dernier_code_retour = 0;
        return;
    }
    
    // C'est une commande externe, la chercher et l'exécuter
    pid_t pid = fork();
    if (pid == 0) {
        // Processus enfant
        char *chemin_commande = chercher_commande_dans_path(arguments[0]);

        if (chemin_commande != NULL) {
            // Remplacer les variables et motifs dans les arguments
            for (int i = 0; arguments[i] != NULL; i++) {
                char *avec_var = remplacer_variable(arguments[i]);
                if (avec_var != arguments[i]) {
                    // Si c'est une référence à une variable, utiliser sa valeur
                    arguments[i] = avec_var;
                } else {
                    // Sinon, chercher les motifs
                    remplacer_motifs(arguments[i]);
                }
            }

            if (execve(chemin_commande, arguments, environ) == -1) {
                afficher_erreur("Impossible d'exécuter la commande", strerror(errno));
            }

            free(chemin_commande);
        } else {
            afficher_erreur("Commande non trouvée", arguments[0]);
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        afficher_erreur("Fork a échoué", strerror(errno));
        dernier_code_retour = 1;
    } else {
        // Processus parent
        if (en_arriere_plan) {
            printf(ANSI_COLOR_YELLOW "[" INFO_ICON "] Processus %d lancé en arrière-plan\n" ANSI_COLOR_RESET, pid);
            dernier_code_retour = 0;
        } else {
            int status;
            waitpid(pid, &status, 0);
            
            if (WIFEXITED(status)) {
                dernier_code_retour = WEXITSTATUS(status);
                if (dernier_code_retour != 0) {
                    afficher_erreur("La commande a retourné une erreur", NULL);
                }
            } else {
                dernier_code_retour = 1;
            }
        }
    }
}

// Fonction pour analyser et exécuter une ligne contenant plusieurs commandes séparées par ;
void analyser_et_executer_ligne(char *ligne) {
    // Supprimer le retour chariot
    ligne[strcspn(ligne, "\n")] = '\0';
    
    // Vérifier si c'est une ligne vide
    if (strlen(ligne) == 0) return;
    
    // Ajouter à l'historique si nous sommes en mode interactif
    if (mode_commande) {
        ajouter_historique(ligne);
    }
    
    // Séparer les commandes par ;
    char *saveptr;
    char *commande = strtok_r(ligne, ";", &saveptr);
    
    while (commande != NULL) {
        // Supprimer les espaces au début
        while (*commande == ' ') commande++;
        
        if (strlen(commande) > 0) {
            // Découper la commande en arguments
            char *arguments[64] = {NULL};
            int en_arriere_plan = 0;
            int i = 0;
            char *saveptr2;
            char *token = strtok_r(commande, " \t", &saveptr2);
            
            while (token != NULL && i < 63) {
                if (strcmp(token, "&") == 0) {
                    en_arriere_plan = 1;
                } else {
                    arguments[i++] = token;
                }
                token = strtok_r(NULL, " \t", &saveptr2);
            }
            arguments[i] = NULL;
            
            // Exécuter la commande
            if (arguments[0] != NULL) {
                executer_commande(arguments, en_arriere_plan);
            }
        }
        
        commande = strtok_r(NULL, ";", &saveptr);
    }
}

// Fonction pour changer de répertoire
void changer_repertoire(char *chemin) {
    // Remplacer ~ par $HOME
    char chemin_reel[TAILLE_MAX_COMMANDE];
    
    if (chemin[0] == '~' && (chemin[1] == '/' || chemin[1] == '\0')) {
        char *home = getenv("HOME");
        if (home) {
            snprintf(chemin_reel, TAILLE_MAX_COMMANDE, "%s%s", home, chemin + 1);
            chemin = chemin_reel;
        }
    }
    
    if (chdir(chemin) == -1) {
        afficher_erreur("Impossible de changer de répertoire", strerror(errno));
        dernier_code_retour = 1;
    } else {
        dernier_code_retour = 0;
    }
}

// Fonction pour afficher le répertoire courant
void afficher_repertoire_courant() {
    char chemin[TAILLE_MAX_COMMANDE];
    if (getcwd(chemin, sizeof(chemin)) != NULL) {
        printf("%s\n", chemin);
        dernier_code_retour = 0;
    } else {
        afficher_erreur("Impossible d'obtenir le répertoire courant", strerror(errno));
        dernier_code_retour = 1;
    }
}

// Fonction pour afficher l'aide avec un style moderne
void afficher_aide() {
    printf("\n");
    printf("  " BG_BLUE ANSI_COLOR_WHITE ANSI_BOLD " mbash " ANSI_COLOR_RESET " " ANSI_COLOR_BLUE "- Mini Shell" ANSI_COLOR_RESET "\n\n");
    
    printf("  " ANSI_BOLD "COMMANDES INTÉGRÉES" ANSI_COLOR_RESET "\n");
    printf("  ┌─────────────────────────────────────────────────────────────┐\n");
    printf("  │ " ANSI_COLOR_GREEN "cd <répertoire>" ANSI_COLOR_RESET "  Changer de répertoire                │\n");
    printf("  │ " ANSI_COLOR_GREEN "pwd" ANSI_COLOR_RESET "             Afficher le répertoire courant        │\n");
    printf("  │ " ANSI_COLOR_GREEN "set <VAR> <val>" ANSI_COLOR_RESET " Définir une variable d'environnement  │\n");
    printf("  │ " ANSI_COLOR_GREEN "echo <texte>" ANSI_COLOR_RESET "     Afficher du texte                    │\n");
    printf("  │ " ANSI_COLOR_GREEN "history" ANSI_COLOR_RESET "         Afficher l'historique des commandes   │\n");
    printf("  │ " ANSI_COLOR_GREEN "help" ANSI_COLOR_RESET "            Afficher cette aide                   │\n");
    printf("  │ " ANSI_COLOR_GREEN "exit" ANSI_COLOR_RESET "            Quitter mbash                         │\n");
    printf("  └─────────────────────────────────────────────────────────────┘\n\n");
    
    printf("  " ANSI_BOLD "RACCOURCIS CLAVIER" ANSI_COLOR_RESET "\n");
    printf("  ┌─────────────────────────────────────────────────────────────┐\n");
    printf("  │ " ANSI_COLOR_YELLOW "Tab" ANSI_COLOR_RESET "             Compléter les commandes et chemins    │\n");
    printf("  │ " ANSI_COLOR_YELLOW "Flèches ↑/↓" ANSI_COLOR_RESET "      Naviguer dans l'historique           │\n");
    printf("  │ " ANSI_COLOR_YELLOW "Flèches ←/→" ANSI_COLOR_RESET "      Déplacer le curseur                  │\n");
    printf("  │ " ANSI_COLOR_YELLOW "Home/End" ANSI_COLOR_RESET "         Aller au début/fin de ligne          │\n");
    printf("  │ " ANSI_COLOR_YELLOW "Ctrl+C" ANSI_COLOR_RESET "           Interrompre la commande en cours     │\n");
    printf("  │ " ANSI_COLOR_YELLOW "Ctrl+D" ANSI_COLOR_RESET "           Quitter mbash (EOF)                  │\n");
    printf("  └─────────────────────────────────────────────────────────────┘\n\n");
    
    printf("  " ANSI_BOLD "FONCTIONNALITÉS" ANSI_COLOR_RESET "\n");
    printf("  ┌─────────────────────────────────────────────────────────────┐\n");
    printf("  │ %s Exécution de commandes externes                        │\n", CHECK_MARK);
    printf("  │ %s Exécution en arrière-plan avec " ANSI_COLOR_YELLOW "&" ANSI_COLOR_RESET "                    │\n", CHECK_MARK);
    printf("  │ %s Variables d'environnement avec " ANSI_COLOR_YELLOW "$" ANSI_COLOR_RESET "                    │\n", CHECK_MARK);
    printf("  │ %s Commandes multiples séparées par " ANSI_COLOR_YELLOW ";" ANSI_COLOR_RESET "                  │\n", CHECK_MARK);
    printf("  │ %s Configuration via ~/.mbashrc                           │\n", CHECK_MARK);
    printf("  │ %s Complétion de chemins et commandes                     │\n", CHECK_MARK);
    printf("  └─────────────────────────────────────────────────────────────┘\n\n");
}

// Gestionnaire de signaux pour SIGCHLD
void gestionnaire_sigchld(int sig) {
    (void)sig; // Éviter les avertissements pour le paramètre inutilisé
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            if (mode_commande) {  // Afficher uniquement en mode interactif
                printf("\n" ANSI_COLOR_CYAN "[" INFO_ICON "] Processus %d terminé avec statut %d\n" ANSI_COLOR_RESET, 
                       pid, WEXITSTATUS(status));
            }
        } else if (WIFSIGNALED(status)) {
            if (mode_commande) {  // Afficher uniquement en mode interactif
                printf("\n" ANSI_COLOR_YELLOW "[" WARNING_ICON "] Processus %d tué par signal %d\n" ANSI_COLOR_RESET, 
                       pid, WTERMSIG(status));
            }
        }
    }
}

// Fonction principale
int main(int argc, char *argv[]) {
    // Configurer le gestionnaire pour SIGCHLD
    struct sigaction sa;
    sa.sa_handler = gestionnaire_sigchld;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    
    // Vérifier si nous devons exécuter un script
    if (argc > 1) {
        // Mode script
        mode_commande = 0;
        FILE *script = fopen(argv[1], "r");
        if (script == NULL) {
            afficher_erreur("Impossible d'ouvrir le script", strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        char ligne[TAILLE_MAX_COMMANDE];
        while (fgets(ligne, TAILLE_MAX_COMMANDE, script) != NULL) {
            analyser_et_executer_ligne(ligne);
        }
        
        fclose(script);
        exit(0);
    }
    
    // Mode interactif
    initialiser_terminal();
    afficher_banniere();
    charger_rc();
    
    while (1) {
        // Affichage du prompt
        char *prompt = construire_prompt();
        printf("%s", prompt);
        fflush(stdout);
        
        // Lecture de la commande avec édition et complétion
        char *ligne = lire_ligne_editee();
        
        if (ligne == NULL) {
            // EOF (Ctrl+D)
            printf("\n");
            break;
        }
        
        if (strlen(ligne) > 0) {
            // Traiter la commande
            analyser_et_executer_ligne(ligne);
        }
    }
    
    restaurer_terminal();
    afficher_info("Au revoir !");
    return 0;
}