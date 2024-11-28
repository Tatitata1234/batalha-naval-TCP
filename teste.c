#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <cjson/cJSON.h>

#define TAMANHO_MAXIMO 10
#define QUANTIDADE_NAVIOS 6
#define MAX_JSON_SIZE 65536

char gradeJogador[TAMANHO_MAXIMO][TAMANHO_MAXIMO];
char gradeAdversario[TAMANHO_MAXIMO][TAMANHO_MAXIMO];
char gradeAdversarioResposta[TAMANHO_MAXIMO][TAMANHO_MAXIMO];
char jogadasFeitas[100][2];
int contJogFeitas = 0;
int contNaviosAbatidos = 0;
int contNaviosAbatidosJog = 0;
bool isServ;
bool isJogadorVez;

typedef struct Navio {
    int xi;
    int yi;
    int xf;
    int yf;
    int tamanho;
    char direcao;
    char tipo[20];
    int posicoes[5][2];
    int contMorte;
    bool morreu;
} Navio;

typedef struct Erro {
    bool isSuccess;
    bool isGanhou;
    char mensagem[50];
    int jogada[2];
} Erro;

Navio naviosJogador[QUANTIDADE_NAVIOS];
Navio naviosAdversario[QUANTIDADE_NAVIOS];

const char* ajustarNome(const char* tipo) {
    if (strcmp(tipo, "Porta-aviões") == 0) {
        return "porta-avioes";
    } else if (strcmp(tipo, "Encouraçado") == 0) {
        return "encouracado";
    } else if (strcmp(tipo, "Cruzador") == 0) {
        return "cruzador";
    } else if (strcmp(tipo, "Destróier") == 0) {
        return "destroier";
    } else {
        return "tipo desconhecido";
    }
}

Erro processaJogada(int coluna, int linha) {
    Erro erro;

    if (coluna < 0 || coluna > 9) {
        erro.isSuccess = false;
        strcpy(erro.mensagem, "Número inválido");
        return erro;
    }

    if (linha < 0 || linha > 9) {
        erro.isSuccess = false;
        strcpy(erro.mensagem, "Número inválido");
        return erro;
    }
    
    erro.jogada[0] = linha;
    erro.jogada[1] = coluna;
    erro.isSuccess = true;
    strcpy(erro.mensagem, "");
    return erro;
}

Erro posicionaNavio(int coluna, int linha, char direcao, 
    int tamanho, int posicao, char *nome
) {
    Erro erro = processaJogada(coluna, linha);

    if (!erro.isSuccess) {
        return erro;
    }
    
    Navio navio;
    navio.xi = coluna;
    navio.yi = linha; 

    if (direcao =='H') {
        navio.xf = navio.xi + tamanho -1;
        if (navio.xf > 9) {
            erro.isSuccess = false;
            strcpy(erro.mensagem,"Navio ultrapassou o limite horizontal do mapa");
            return erro;
        }
        
        navio.yf = linha;
        for (int i = navio.xi; i <= navio.xf; i++) {
            if(gradeJogador[linha][i] == 'N'){
                erro.isSuccess = false;
                strcpy(erro.mensagem, "Navios não podem se cruzar");
                return erro;
            }
        }
        for (int i = navio.xi; i <= navio.xf; i++) {
            gradeJogador[linha][i] = 'N';
        }
    } else if (direcao =='V') {
        navio.yf = linha + tamanho - 1;
        if (navio.yf > 9) {
            erro.isSuccess = false;
            strcpy(erro.mensagem, "Navio ultrapassou o limite vertical do mapa");
            return erro;
        }
        navio.xf = navio.xi;
        for (int i = linha; i <= navio.yf; i++) {
            if (gradeJogador[i][navio.xi] == 'N') {
                erro.isSuccess = false;
                strcpy(erro.mensagem, "Navios não podem se cruzar");
                return erro;
            }
        }
        for (int i = linha; i <= navio.yf; i++) {
            gradeJogador[i][navio.xi] = 'N';
        }
    } else {
        erro.isSuccess = false;
        strcpy(erro.mensagem, "Direção inválida");
        return erro;
    }

    navio.direcao = direcao;
    navio.tamanho = tamanho;
    strcpy(navio.tipo, ajustarNome(nome));
    
    erro.isSuccess = true;

    for (int i = 0; i < tamanho; i++) {
        if (direcao == 'H') {
            
            navio.posicoes[i][0] = linha;
            navio.posicoes[i][1] = navio.xi + i;
            //printf("%d",navio.posicoes[i][0]);
            //printf("%d",navio.posicoes[i][1]);
        } else if (direcao == 'V') {
            navio.posicoes[i][0] = linha + i;
            navio.posicoes[i][1] = navio.xi;
        }
    }

    naviosJogador[posicao] = navio;
    strcpy(erro.mensagem, "");
    return erro;    
}

void seuMapa() {
    printf("------Seu mapa-------\n");
    printf("  0 1 2 3 4 5 6 7 8 9\n");
    for (int i = 0; i < TAMANHO_MAXIMO; i++) {
        printf("%d ", i);
        for (int j = 0; j < TAMANHO_MAXIMO; j++)
        {
            printf("%c ", gradeJogador[i][j]);
        }
        printf("\n");
    }
}

int converterJsonParaNavios(const char *jsonStr) {
    
    cJSON *json = cJSON_Parse(jsonStr);
    if (!json) {
        fprintf(stderr, "Erro ao analisar o JSON\n");
        return -1;
    }

    int navioCount = 0;
    cJSON *navioArray = json;
    cJSON *navioItem;

    cJSON_ArrayForEach(navioItem, navioArray) {
        if (navioCount >= QUANTIDADE_NAVIOS) break;
        memset(&naviosAdversario[navioCount], 0, sizeof(naviosAdversario[navioCount]));

        cJSON *tipo = cJSON_GetObjectItemCaseSensitive(navioItem, "tipo");
        cJSON *posicoes = cJSON_GetObjectItemCaseSensitive(navioItem, "posicoes");

        if (!cJSON_IsArray(posicoes) || !cJSON_IsString(tipo)) continue;

        int tamanho = cJSON_GetArraySize(posicoes);
        if (tamanho > 5) { // Garantir que o array cabe no struct
            fprintf(stderr, "Número de posições excede o máximo permitido para o navio %d.\n", navioCount + 1);
            continue;
        }

        for (int i = 0; i < tamanho; i++) {
            cJSON *posicao = cJSON_GetArrayItem(posicoes, i);
            if (!cJSON_IsArray(posicao) || cJSON_GetArraySize(posicao) < 2) continue;

            naviosAdversario[navioCount].posicoes[i][0] = cJSON_GetArrayItem(posicao, 0)->valueint; // Linha
            naviosAdversario[navioCount].posicoes[i][1] = cJSON_GetArrayItem(posicao, 1)->valueint; // Coluna
        }

        cJSON *primeira = cJSON_GetArrayItem(posicoes, 0);
        cJSON *ultima = cJSON_GetArrayItem(posicoes, cJSON_GetArraySize(posicoes) - 1);

        if (!primeira || !ultima) continue;

        naviosAdversario[navioCount].xi = cJSON_GetArrayItem(primeira, 1)->valueint;
        naviosAdversario[navioCount].yi = cJSON_GetArrayItem(primeira, 0)->valueint;
        naviosAdversario[navioCount].xf = cJSON_GetArrayItem(ultima, 1)->valueint;
        naviosAdversario[navioCount].yf = cJSON_GetArrayItem(ultima, 0)->valueint;
        naviosAdversario[navioCount].tamanho = cJSON_GetArraySize(posicoes);
        naviosAdversario[navioCount].contMorte = 0;
        naviosAdversario[navioCount].morreu = false;
        naviosAdversario[navioCount].direcao = (naviosAdversario[navioCount].xi == naviosAdversario[navioCount].xf) ? 'V' : 'H';
        strncpy(naviosAdversario[navioCount].tipo, tipo->valuestring, sizeof(naviosAdversario[navioCount].tipo) - 1);
        navioCount++;
    }

    cJSON_Delete(json);
    return navioCount;
}

char* gerarJSONComCJSON() {
    cJSON *array = cJSON_CreateArray();

    for (int i = 0; i < QUANTIDADE_NAVIOS; i++) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "tipo", naviosJogador[i].tipo);

        cJSON *posicoes = cJSON_CreateArray();
        int dx = (naviosJogador[i].direcao == 'H') ? 1 : 0;
        int dy = (naviosJogador[i].direcao == 'V') ? 1 : 0;
        for (int j = 0; j < naviosJogador[i].tamanho; j++) {
            cJSON *posicao = cJSON_CreateArray();
            cJSON_AddItemToArray(posicao, cJSON_CreateNumber(naviosJogador[i].yi + j * dy));
            cJSON_AddItemToArray(posicao, cJSON_CreateNumber(naviosJogador[i].xi + j * dx));
            cJSON_AddItemToArray(posicoes, posicao);
        }

        cJSON_AddItemToObject(obj, "posicoes", posicoes);
        cJSON_AddItemToArray(array, obj);
    }

    return cJSON_Print(array);
}

void preencheComZerosGrades() {
    for (int i = 0; i < TAMANHO_MAXIMO; i++) {
        for (int j = 0; j < TAMANHO_MAXIMO; j++) {
            gradeJogador[i][j] = '~';
            gradeAdversario[i][j] = '~';
            gradeAdversarioResposta[i][j] = '~';
        }
    }
}

void preencheComZerosGradeJog() {
    for (int i = 0; i < TAMANHO_MAXIMO; i++) {
        for (int j = 0; j < TAMANHO_MAXIMO; j++) {
            gradeJogador[i][j] = '~';
        }
    }
}

void marcarNaviosAdversariosPosIncializacao() {
    for (int n = 0; n < QUANTIDADE_NAVIOS; n++) {
        Navio *navio = &naviosAdversario[n];
        // Marcar navios horizontais
        if (navio->direcao == 'H') {
            for (int x = navio->xi; x <= navio->xf; x++) {
                gradeAdversarioResposta[navio->yi][x] = 'N';
            }
        }
        // Marcar navios verticais
        else if (navio->direcao == 'V') {
            for (int y = navio->yi; y <= navio->yf; y++) {
                gradeAdversarioResposta[y][navio->xi] = 'N';
            }
        }
    }
}

void recebeNavio(char *nome, int tamanho, int posicao) {
    
    bool controle = true;
    do {
        printf("\n\n");
        printf("Posicione o %s:\n", nome);

        int coluna = -1;
        int linha = -1;
        char dir = 'X';
        scanf(" %1d%1d%1c", &linha, &coluna, &dir);
        Erro erro = posicionaNavio(coluna, linha, (char)dir, tamanho, posicao,nome);
        if (!erro.isSuccess) {
            printf(erro.mensagem);
            printf("\n");
        } else {
            controle = false;
        }

    } while (controle);
}

void inicializaNaviosJogador() {
    system("clear");
    preencheComZerosGradeJog();
    seuMapa();
    printf("Antes de começar o jogo vamos posicionar os Navios!\n");
    printf("Você precisa posicionar 6 Navios no total!\n");
    printf("1 porta-aviões (5 células)\n");
    printf("1 encouraçado (4 células)\n");
    printf("2 cruzadores (3 células cada)\n");
    printf("2 destróieres (2 células cada)\n");
    printf("Inicio tutorial!\n");
    printf("\nVamos começar pelos maiores até os menores\n");
    printf("Posicione o Porta-aviões\n");
    printf("Digite a linha e coluna correspondente da posição inicial\n");
    printf("e a orientação do Navio H - horizontal e V - vertical\n");
    printf("O Navio ficará posicionado nessa posição inicial e o \nresto do seu corpo será colocado na direita ou para baixo\n");
    printf("Exemplo 00H\n");
    posicionaNavio(0, 0, 'H', 5, 0, "Porta-aviões");
    seuMapa();
    preencheComZerosGradeJog();
    printf("Fim do tutorial\n\n\n");

    seuMapa();
    recebeNavio("Porta-aviões", 5, 0);
    system("clear");
    seuMapa();
    recebeNavio("Encouraçado", 4, 1);
    system("clear");
    seuMapa();

    int i = 0;
    do {
        recebeNavio("Cruzador", 3, (2 + i));
        system("clear");
        i++;
        seuMapa();
    } while (i < 2);
    i = 0;
    do {
        recebeNavio("Destróier", 2, (4 + i));
        system("clear");
        i++;
        seuMapa();
    } while (i < 2);
    system("clear");
}

void telaJogo() {
    system("clear");
    printf("---------------------\n");
    printf("----Batalha Naval----\n");
    printf("---------------------\n");
    seuMapa();
    printf("---------------------\n");
    printf("-----Adversário------\n");
    printf("  0 1 2 3 4 5 6 7 8 9\n");
    for (int i = 0; i < TAMANHO_MAXIMO; i++) {
        printf("%d ", i);
        for (int j = 0; j < TAMANHO_MAXIMO; j++) {
            printf("%c ", gradeAdversario[i][j]);   
        }
        printf("\n");
    }
}

Erro processaTiro(int l, int c, 
    char gradeRes[TAMANHO_MAXIMO][TAMANHO_MAXIMO],
    char grade[TAMANHO_MAXIMO][TAMANHO_MAXIMO],
    Navio navio[QUANTIDADE_NAVIOS],
    bool isAdv
){
    Erro erro;
    if (!isAdv) {
        for (int i = 0; i < contJogFeitas; i++) {
            if (jogadasFeitas[i][0] == l && jogadasFeitas[i][1] == c) {
                erro.isSuccess = false;
                strcpy(erro.mensagem, "Essa jogada já foi feita");
                return erro;
            }
        }
    }
    
    if (gradeRes[l][c] == 'N') {
        for (int i = 0; i < QUANTIDADE_NAVIOS; i++) {
            Navio *n = &navio[i];
            for (int j = 0; j < n->tamanho; j++) {
                if (n->posicoes[j][0] == l && n->posicoes[j][1] == c) {
                    n->contMorte++;
                    if (n->contMorte == n->tamanho) {
                        n->morreu = true;
                        snprintf(erro.mensagem, sizeof(erro.mensagem), "Destruiu %s", n->tipo);
                        if (!isAdv) {
                            contNaviosAbatidos++;
                        } else {
                            contNaviosAbatidosJog++;
                        }
                    } else {
                        if (isAdv) {
                            snprintf(erro.mensagem, sizeof(erro.mensagem), "Adversário acertou %s", n->tipo);
                        } else {
                            strcpy(erro.mensagem, "Acertou");
                        }
                    }
                    erro.isSuccess = true;
                    grade[l][c] = 'X';

                    break;
                }
            }
        }
    } else {
        grade[l][c] = 'O';
        if (isAdv) {
            strcpy(erro.mensagem, "Tiro do adversário caiu na água");
        } else {
            strcpy(erro.mensagem, "Caiu na água");
        }
        erro.isSuccess = true;
    }
    if (!isAdv) {
        jogadasFeitas[contJogFeitas][0] = l;
        jogadasFeitas[contJogFeitas][1] = c;
        contJogFeitas++;    
    }
    return erro;
}

Erro validaGanhou(bool isAdv) {
    Erro erro;
    if (contNaviosAbatidos == QUANTIDADE_NAVIOS) {
        erro.isSuccess = true;
        erro.isGanhou = true;
        strcpy(erro.mensagem, "Você ganhou o jogo!");
        return erro;
    }
    
    if (contNaviosAbatidosJog == QUANTIDADE_NAVIOS) {
        erro.isSuccess = true;
        erro.isGanhou = true;
        strcpy(erro.mensagem, "Você perdeu o jogo!");
        return erro;
    }
    erro.isGanhou = false;
    erro.isSuccess = false;
    return erro;
}

char* gerarJsonComVariaveis() {
    cJSON *root = cJSON_CreateObject();

    cJSON_AddBoolToObject(root, "isServ", isServ);
    cJSON_AddBoolToObject(root, "isJogadorVez", isJogadorVez);

    cJSON *gradeJogadorJson = cJSON_CreateArray();
    cJSON *gradeAdversarioJson = cJSON_CreateArray();
    cJSON *gradeAdversarioRespostaJson = cJSON_CreateArray();

    for (int i = 0; i < TAMANHO_MAXIMO; i++) {
        cJSON *linhaJogador = cJSON_CreateArray();
        cJSON *linhaAdversario = cJSON_CreateArray();
        cJSON *linhaResposta = cJSON_CreateArray();

        for (int j = 0; j < TAMANHO_MAXIMO; j++) {
            cJSON_AddItemToArray(linhaJogador, cJSON_CreateString((char[]){gradeJogador[i][j], '\0'}));
            cJSON_AddItemToArray(linhaAdversario, cJSON_CreateString((char[]){gradeAdversario[i][j], '\0'}));
            cJSON_AddItemToArray(linhaResposta, cJSON_CreateString((char[]){gradeAdversarioResposta[i][j], '\0'}));
        }

        cJSON_AddItemToArray(gradeJogadorJson, linhaJogador);
        cJSON_AddItemToArray(gradeAdversarioJson, linhaAdversario);
        cJSON_AddItemToArray(gradeAdversarioRespostaJson, linhaResposta);
    }

    cJSON_AddItemToObject(root, "gradeJogador", gradeJogadorJson);
    cJSON_AddItemToObject(root, "gradeAdversario", gradeAdversarioJson);
    cJSON_AddItemToObject(root, "gradeAdversarioResposta", gradeAdversarioRespostaJson);

    cJSON *jogadasJson = cJSON_CreateArray();
    for (int i = 0; i < contJogFeitas; i++) {
        cJSON *jogada = cJSON_CreateArray();
        cJSON_AddItemToArray(jogada, cJSON_CreateNumber(jogadasFeitas[i][0]));
        cJSON_AddItemToArray(jogada, cJSON_CreateNumber(jogadasFeitas[i][1]));
        cJSON_AddItemToArray(jogadasJson, jogada);
    }
    cJSON_AddItemToObject(root, "jogadasFeitas", jogadasJson);

    cJSON_AddNumberToObject(root, "contJogFeitas", contJogFeitas);
    cJSON_AddNumberToObject(root, "contNaviosAbatidos", contNaviosAbatidos);
    cJSON_AddNumberToObject(root, "contNaviosAbatidosJog", contNaviosAbatidosJog);

    cJSON* navioToJson(const Navio *navio) {
        cJSON *jsonNavio = cJSON_CreateObject();
        cJSON_AddNumberToObject(jsonNavio, "xi", navio->xi);
        cJSON_AddNumberToObject(jsonNavio, "yi", navio->yi);
        cJSON_AddNumberToObject(jsonNavio, "xf", navio->xf);
        cJSON_AddNumberToObject(jsonNavio, "yf", navio->yf);
        cJSON_AddNumberToObject(jsonNavio, "tamanho", navio->tamanho);
        cJSON_AddStringToObject(jsonNavio, "direcao", (char[]){navio->direcao, '\0'});
        cJSON_AddStringToObject(jsonNavio, "tipo", navio->tipo);

        cJSON *posicoesJson = cJSON_CreateArray();
        for (int i = 0; i < navio->tamanho; i++) {
            cJSON *posicao = cJSON_CreateArray();
            cJSON_AddItemToArray(posicao, cJSON_CreateNumber(navio->posicoes[i][0]));
            cJSON_AddItemToArray(posicao, cJSON_CreateNumber(navio->posicoes[i][1]));
            cJSON_AddItemToArray(posicoesJson, posicao);
        }
        cJSON_AddItemToObject(jsonNavio, "posicoes", posicoesJson);

        cJSON_AddNumberToObject(jsonNavio, "contMorte", navio->contMorte);
        cJSON_AddBoolToObject(jsonNavio, "morreu", navio->morreu);

        return jsonNavio;
    }

    cJSON *naviosJogadorJson = cJSON_CreateArray();
    cJSON *naviosAdversarioJson = cJSON_CreateArray();

    for (int i = 0; i < QUANTIDADE_NAVIOS; i++) {
        cJSON_AddItemToArray(naviosJogadorJson, navioToJson(&naviosJogador[i]));
        cJSON_AddItemToArray(naviosAdversarioJson, navioToJson(&naviosAdversario[i]));
    }

    cJSON_AddItemToObject(root, "naviosJogador", naviosJogadorJson);
    cJSON_AddItemToObject(root, "naviosAdversario", naviosAdversarioJson);

    char *jsonString = cJSON_Print(root);
    cJSON_Delete(root);

    return jsonString; 
}

void salvarJSONEmArquivo(char *nomeArquivo) {
    char *jsonString = gerarJsonComVariaveis();
    if (!jsonString) {
        fprintf(stderr, "Erro ao gerar o JSON\n");
        return;
    }
    // Abre o arquivo para escrita
    FILE *arquivo = fopen(nomeArquivo, "w");
    if (!arquivo) {
        perror("Erro ao abrir o arquivo para escrita");
        free(jsonString);
        return;
    }

    fprintf(arquivo, "%s\n", jsonString);

    // Libera a memória alocada para o JSON e fecha o arquivo
    fclose(arquivo);
    free(jsonString);
}

Erro jogar(int loc_newsockfd) {
    Erro erroGanhou;

    bool controle = true;
    char jogada[3];
    
    printf("Sua vez de jogar...\n");
    do {
        int coluna = -1;
        int linha = -1;
        scanf(" %1d%1d", &linha, &coluna);
        Erro erro = processaJogada(coluna, linha);
        if (!erro.isSuccess) {
            printf(erro.mensagem);
            printf("\n");
        } else {
            sprintf(jogada, "%d%d", erro.jogada[0], erro.jogada[1]);
            int l = erro.jogada[0];
            int c = erro.jogada[1];
            Erro erro = processaTiro(
                linha, coluna,
                gradeAdversarioResposta,
                gradeAdversario,
                naviosAdversario,
                false
            );
            if (erro.isSuccess) {
                controle = false;
            }
            erroGanhou = validaGanhou(false);
            telaJogo();
            if (erroGanhou.isSuccess) {
                printf(erroGanhou.mensagem);
            } else {
                printf(erro.mensagem);
                erroGanhou.isSuccess = true;
            }
            printf("\n");
        }
    } while (controle);
    isJogadorVez = false;
    if (send(loc_newsockfd, jogada, strlen(jogada), 0) < 0) {
        close(loc_newsockfd);
        Erro e;
        e.isSuccess = false;
        e.isGanhou = false;
        return e;
    }
    salvarJSONEmArquivo("dados1.json");
    return erroGanhou;
}

Erro receberJogada(int rem_sockfd) {
    printf("Esperando o adversário jogar...\n");
    char jogada[3];
    if (recv(rem_sockfd, &jogada, sizeof(jogada), 0) < 0) {
        close(rem_sockfd);
        Erro e;
        e.isSuccess = false;
        e.isGanhou = false;
        return e;
    }

    int l = (jogada[0] - '0');
    int c = (jogada[1] - '0');
    Erro erro = processaTiro(
        l, c,
        gradeJogador,
        gradeJogador,
        naviosJogador,
        true
    );
    Erro erroGanhou = validaGanhou(true);
    telaJogo();
    if (erroGanhou.isSuccess) {
        printf(erroGanhou.mensagem);
    } else {
        printf(erro.mensagem);
        erroGanhou.isSuccess = true;
    }
    printf("\n");
    isJogadorVez = true;
    salvarJSONEmArquivo("dados1.json");
    return erroGanhou;
}

Erro mainServidor() {
    int sock;
    struct sockaddr_in me, from;
    socklen_t adl = sizeof(from);

    int loc_sockfd, loc_newsockfd, tamanho;
    char linha[1000];
    /* Estrutura: familia + endereco IP + porta */
    struct sockaddr_in loc_addr;
    loc_sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (loc_sockfd < 0) {
        perror("Criando stream socket");
        Erro e;
        e.isSuccess = false;
        return e;
    }

    /* Construcao da estrutura do endereco local */
    /* Preenchendo a estrutura socket loc_addr (familia, IP, porta) */
    loc_addr.sin_family = AF_INET;         /* familia do protocolo*/
    loc_addr.sin_addr.s_addr = INADDR_ANY; /* endereco IP local */
    loc_addr.sin_port = htons(8080);       /* porta local  */
    bzero(&(loc_addr.sin_zero), 8);

    /* Bind para o endereco local*/
    /* parametros(descritor socket, estrutura do endereco local, comprimento do endereco) */
    if (bind(loc_sockfd, (struct sockaddr *)&loc_addr, sizeof(struct sockaddr)) < 0) {
        perror("Ligando stream socket");
        Erro e;
        e.isSuccess = false;
        return e;
    }

    /* parametros(descritor socket,
    numeros de conexoes em espera sem serem aceites pelo accept)*/
    listen(loc_sockfd, 1);

    tamanho = sizeof(struct sockaddr_in);
    /* Accept permite aceitar um pedido de conexao, devolve um novo "socket" ja ligado ao emissor do pedido e o "socket" original*/
    /* parametros(descritor socket, estrutura do endereco local, comprimento do endereco)*/
    loc_newsockfd = accept(loc_sockfd, (struct sockaddr *)&loc_addr, &tamanho);

    inicializaNaviosJogador();
    char *json = gerarJSONComCJSON();

    // Envia o JSON para o cliente assim que a conexão é estabelecida
    if (send(loc_newsockfd, json, sizeof(linha), 0) < 0) {
        perror("Erro ao enviar JSON para o cliente");
        close(loc_sockfd);
        close(loc_newsockfd);
        free(json);
        Erro e;
        e.isSuccess = false;
        return e;
    }
    telaJogo();
    printf("Esperando adversário posicionar os Navios...\n");
    if (recv(loc_newsockfd, &linha, sizeof(linha), 0) < 0) {
        close(loc_sockfd);
        close(loc_newsockfd);
        free(json);
        Erro e;
        e.isSuccess = false;
        return e;
    }
    char *jsonAdv = linha;
    converterJsonParaNavios(jsonAdv);
    marcarNaviosAdversariosPosIncializacao();
    free(json); // Liberar a memória alocada para o JSON

    telaJogo();
    printf("Você começa jogando...\n");
    printf("Envie a posição em que quer disparar o tiro\n");
    printf("Exemplo 22\n");

    do {
        Erro er1 = jogar(loc_newsockfd);
        if (er1.isGanhou) {
            break;
        }
        Erro er2 = receberJogada(loc_newsockfd);
        if (er2.isGanhou) {
            break;
        }
    } while (true);

    close(loc_sockfd);
    close(loc_newsockfd);
}

Erro mainCliente() {
    char *rem_hostname;
    int rem_port;
    /* Estrutura: familia + endereco IP + porta */
    struct sockaddr_in rem_addr;
    int rem_sockfd;

    char ipServ[INET_ADDRSTRLEN];
    printf("Digite o IP do adversário:\n");
    scanf("%s", ipServ);
    rem_hostname = ipServ;
    rem_port = 8080;
    rem_addr.sin_family = AF_INET; /* familia do protocolo*/
    rem_addr.sin_addr.s_addr = inet_addr(rem_hostname); /* endereco IP local */
    rem_addr.sin_port = htons(rem_port); /* porta local  */

    rem_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (rem_sockfd < 0) {
        perror("Criando stream socket");
        Erro e;
        e.isSuccess = false;
        return e;
    }

    if (connect(rem_sockfd, (struct sockaddr *) &rem_addr, sizeof(rem_addr)) < 0) {
        perror("Conectando stream socket");
        Erro e;
        e.isSuccess = false;
        return e;
    }
    telaJogo();
    printf("Esperando adversário posicionar os Navios...\n");

    char linha[1000];
    if (recv(rem_sockfd, linha, sizeof(linha), 0) < 0 ) {
        Erro e;
        e.isSuccess = false;
        return e;
    }

    char *jsonAdv = linha;
    converterJsonParaNavios(jsonAdv);
    marcarNaviosAdversariosPosIncializacao();

    inicializaNaviosJogador();
    char *json = gerarJSONComCJSON();

    if (send(rem_sockfd, json, strlen(json), 0) < 0) {
        perror("Erro ao enviar JSON para o servidor");
        close(rem_sockfd);
        free(json);
        Erro e;
        e.isSuccess = false;
        return e;
    }

    free(json);
    do {
        Erro er2 = receberJogada(rem_sockfd);
        if (er2.isGanhou) {
            return er2;
        }
        Erro er1 = jogar(rem_sockfd);
        if (er1.isGanhou) {
            return er1;
        }
    } while (true);

    close(rem_sockfd);
}

void preencherGrade(char grade[TAMANHO_MAXIMO][TAMANHO_MAXIMO], cJSON *jsonGrade) {
    for (int i = 0; i < TAMANHO_MAXIMO; i++) {
        cJSON *linha = cJSON_GetArrayItem(jsonGrade, i);
        for (int j = 0; j < TAMANHO_MAXIMO; j++) {
            const char *celula = cJSON_GetArrayItem(linha, j)->valuestring;
            grade[i][j] = celula[0]; 
        }
    }
}

void preencherNavios(Navio *navios, cJSON *jsonNavios) {
    for (int i = 0; i < QUANTIDADE_NAVIOS; i++) {
        cJSON *navio = cJSON_GetArrayItem(jsonNavios, i);

        navios[i].xi = cJSON_GetObjectItem(navio, "xi")->valueint;
        navios[i].yi = cJSON_GetObjectItem(navio, "yi")->valueint;
        navios[i].xf = cJSON_GetObjectItem(navio, "xf")->valueint;
        navios[i].yf = cJSON_GetObjectItem(navio, "yf")->valueint;
        navios[i].tamanho = cJSON_GetObjectItem(navio, "tamanho")->valueint;
        navios[i].direcao = cJSON_GetObjectItem(navio, "direcao")->valuestring[0];
        strcpy(navios[i].tipo, cJSON_GetObjectItem(navio, "tipo")->valuestring);
        navios[i].contMorte = cJSON_GetObjectItem(navio, "contMorte")->valueint;
        navios[i].morreu = cJSON_GetObjectItem(navio, "morreu")->valueint;

        cJSON *posicoes = cJSON_GetObjectItem(navio, "posicoes");
        for (int j = 0; j < navios[i].tamanho; j++) {
            cJSON *posicao = cJSON_GetArrayItem(posicoes, j);
            navios[i].posicoes[j][0] = cJSON_GetArrayItem(posicao, 0)->valueint;
            navios[i].posicoes[j][1] = cJSON_GetArrayItem(posicao, 1)->valueint;
        }
    }
}

void preencherVariaveisComJson(const char *jsonString) {
    cJSON *json = cJSON_Parse(jsonString);
    if (!json) {
        printf("Erro ao fazer o parse do JSON.\n");
        return;
    }
    // Preenchendo as grades
    preencherGrade(gradeJogador, cJSON_GetObjectItem(json, "gradeJogador"));
    preencherGrade(gradeAdversario, cJSON_GetObjectItem(json, "gradeAdversario"));
    preencherGrade(gradeAdversarioResposta, cJSON_GetObjectItem(json, "gradeAdversarioResposta"));
    // Preenchendo os navios
    preencherNavios(naviosJogador, cJSON_GetObjectItem(json, "naviosJogador"));
    preencherNavios(naviosAdversario, cJSON_GetObjectItem(json, "naviosAdversario"));
    // Preenchendo as jogadas feitas
    cJSON *jsonJogadas = cJSON_GetObjectItem(json, "jogadasJogador");
    contJogFeitas = cJSON_GetArraySize(jsonJogadas);
    for (int i = 0; i < contJogFeitas; i++) {
        cJSON *jogada = cJSON_GetArrayItem(jsonJogadas, i);
        jogadasFeitas[i][0] = cJSON_GetArrayItem(jogada, 0)->valueint;
        jogadasFeitas[i][1] = cJSON_GetArrayItem(jogada, 1)->valueint;
    }
    // Preenchendo os contadores
    contNaviosAbatidos = cJSON_GetObjectItem(json, "contNaviosAbatidos")->valueint;
    contNaviosAbatidosJog = cJSON_GetObjectItem(json, "contNaviosAbatidosJog")->valueint;
    cJSON *item = cJSON_GetObjectItem(json, "isJogadorVez");
    if (cJSON_IsBool(item)) {  // Verifica se é booleano
        isJogadorVez = cJSON_IsTrue(item);  // Retorna true ou false
    } else {
        printf("Erro: 'isJogadorVez' não é um booleano válido no JSON.\n");
    }
    item = cJSON_GetObjectItem(json, "isServ");
    if (cJSON_IsBool(item)) {  // Verifica se é booleano
        isServ = cJSON_IsTrue(item);  // Retorna true ou false
    } else {
        printf("Erro: 'isServ' não é um booleano válido no JSON.\n");
    }
    cJSON_Delete(json);
}

void lerJsonDoArquivo(const char *nomeArquivo) {
    FILE *arquivo = fopen(nomeArquivo, "r");
    if (arquivo == NULL) {
        printf("Erro ao abrir o arquivo %s\n", nomeArquivo);
        return;
    }

    char *conteudoJson = malloc(MAX_JSON_SIZE);
    if (conteudoJson == NULL) {
        printf("Erro ao alocar memória para o JSON.\n");
        fclose(arquivo);
        return;
    }

    size_t bytesLidos = fread(conteudoJson, 1, MAX_JSON_SIZE - 1, arquivo);
    conteudoJson[bytesLidos] = '\0'; 

    fclose(arquivo);

    preencherVariaveisComJson(conteudoJson);

    free(conteudoJson);
}

void resetGame() {
    if (isServ) {
        int sock;
        struct sockaddr_in me, from;
        socklen_t adl = sizeof(from);

        int loc_sockfd, loc_newsockfd, tamanho;
        char linha[1000];
        /* Estrutura: familia + endereco IP + porta */
        struct sockaddr_in loc_addr;
        loc_sockfd = socket(AF_INET, SOCK_STREAM, 0);

        if (loc_sockfd < 0) {
            perror("Criando stream socket");
            exit(1);
        }

        /* Construcao da estrutura do endereco local */
        /* Preenchendo a estrutura socket loc_addr (familia, IP, porta) */
        loc_addr.sin_family = AF_INET;         /* familia do protocolo*/
        loc_addr.sin_addr.s_addr = INADDR_ANY; /* endereco IP local */
        loc_addr.sin_port = htons(8080);       /* porta local  */
        bzero(&(loc_addr.sin_zero), 8);

        /* Bind para o endereco local*/
        /* parametros(descritor socket, estrutura do endereco local, comprimento do endereco) */
        if (bind(loc_sockfd, (struct sockaddr *)&loc_addr, sizeof(struct sockaddr)) < 0) {
            perror("Ligando stream socket");
            exit(1);
        }

        /* parametros(descritor socket,
        numeros de conexoes em espera sem serem aceites pelo accept)*/
        listen(loc_sockfd, 1);

        tamanho = sizeof(struct sockaddr_in);
        /* Accept permite aceitar um pedido de conexao, devolve um novo "socket" ja ligado ao emissor do pedido e o "socket" original*/
        /* parametros(descritor socket, estrutura do endereco local, comprimento do endereco)*/
        loc_newsockfd = accept(loc_sockfd, (struct sockaddr *)&loc_addr, &tamanho);

        if (isJogadorVez) {
            do {
                Erro er1 = jogar(loc_newsockfd);
                if (er1.isGanhou) {
                    break;
                }
                Erro er2 = receberJogada(loc_newsockfd);
                if (er2.isGanhou) {
                    break;
                }
            } while (true);
        }
        else {
            do {
                Erro er2 = receberJogada(loc_newsockfd);
                if (er2.isGanhou) {
                    break;
                }
                Erro er1 = jogar(loc_newsockfd);
                if (er1.isGanhou) {
                    break;
                }
            } while (true);
        }
        close(loc_sockfd);
        close(loc_newsockfd);
    }
    else {
        char *rem_hostname;
        int rem_port;
        /* Estrutura: familia + endereco IP + porta */
        struct sockaddr_in rem_addr;
        int rem_sockfd;

        char ipServ[INET_ADDRSTRLEN];
        printf("Digite o IP do adversário:\n");
        scanf("%s", ipServ);
        rem_hostname = ipServ;
        rem_port = 8080;
        rem_addr.sin_family = AF_INET; /* familia do protocolo*/
        rem_addr.sin_addr.s_addr = inet_addr(rem_hostname); /* endereco IP local */
        rem_addr.sin_port = htons(rem_port); /* porta local  */

        rem_sockfd = socket(AF_INET, SOCK_STREAM, 0);
        
        if (rem_sockfd < 0) {
            perror("Criando stream socket");
            exit(1);
        }

        if (connect(rem_sockfd, (struct sockaddr *) &rem_addr, sizeof(rem_addr)) < 0) {
            perror("Conectando stream socket");
            exit(1);
        }
        if (isJogadorVez) {
            do {
                Erro er1 = jogar(rem_sockfd);
                if (er1.isGanhou) {
                    break;
                }
                Erro er2 = receberJogada(rem_sockfd);
                if (er2.isGanhou) {
                    break;
                }
            } while (true);
        }
        else {
            do {
                Erro er2 = receberJogada(rem_sockfd);
                if (er2.isGanhou) {
                    break;
                }
                Erro er1 = jogar(rem_sockfd);
                if (er1.isGanhou) {
                    break;
                }
            } while (true);
        }
        close(rem_sockfd);
    }
}

int main(int argc, char *argv[]) {
    int controleMenu = 0;
    system("clear");
    do {
        printf("---------------------\n");
        printf("----Batalha Naval----\n");
        printf("---------------------\n");
        printf("1 - Iniciar a partida\n");
        printf("2 - Entrar em uma partida\n");
        printf("3 - Entrar em uma partida salva\n");
        printf("4 - Sair\n");
        scanf("%d", &controleMenu);

        preencheComZerosGrades();
        switch (controleMenu) {
            case 1:
                isServ = true;
                isJogadorVez = true;
                printf("Esperando o adversário conectar...\n");
                Erro es = mainServidor();
                if (!es.isSuccess) {
                    salvarJSONEmArquivo("dados.json");
                }
                break;
            case 2:
                isServ = false;
                isJogadorVez = false;
                Erro ec = mainCliente();
                if (!ec.isSuccess) {
                    salvarJSONEmArquivo("dados.json");
                }
                break;
            case 3:
                char *nomeArquivo = "dados.json";
                lerJsonDoArquivo(nomeArquivo);
                resetGame();
                break;
            case 4:
                printf("Saindo\n");
                printf("Até a próxima!\n");
                break;
            default:
                printf("Opção não permitida!\n");
                printf("Tente novamente\n");
                break;
        }
        
    } while (controleMenu!=4);
    exit(0);
}

