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
#include <pthread.h>
#include <cjson/cJSON.h>

#define TAMANHO_MAXIMO 10
#define QUANTIDADE_NAVIOS 6

char gradeJogador[TAMANHO_MAXIMO][TAMANHO_MAXIMO];
char gradeAdversario[TAMANHO_MAXIMO][TAMANHO_MAXIMO];
char gradeAdversarioResposta[TAMANHO_MAXIMO][TAMANHO_MAXIMO];
char letras[10] = {'A','B','C','D','E','F','G','H','I','J'}; 
char jogadasFeitas[100][2];
int contJogFeitas = 0;
int contNaviosAbatidos = 0;
int contNaviosAbatidosJog = 0;

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
    char mensagem[50];
    int aux;
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
    //printf("marcar adversario\n");
    for (int n = 0; n < QUANTIDADE_NAVIOS; n++) {
        Navio *navio = &naviosAdversario[n];
        // Marcar navios horizontais
        //printf("marcar adversario for 1\n");
        if (navio->direcao == 'H') {
            //printf("marcar adversario for 1 if 1\n");
            for (int x = navio->xi; x <= navio->xf; x++) {
                //printf("marcar adversario for 1 if 1 for 2\n");
                //printf("%d %d\n",navio->xi,navio->xf);
                gradeAdversarioResposta[navio->yi][x] = 'N';
                //printf("%c\n",gradeAdversarioResposta[navio->yi][x]);
            }
        }
        // Marcar navios verticais
        else if (navio->direcao == 'V') {
            //printf("marcar adversario for 1 if 2\n");
            for (int y = navio->yi; y <= navio->yf; y++) {
                //printf("marcar adversario for 1 if 2 for 3\n");
                //printf("%d %d\n",navio->yi,navio->yf);
                gradeAdversarioResposta[y][navio->xi] = 'N';
                //printf("%c\n",gradeAdversarioResposta[y][navio->xi]);
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
        scanf(" %d %d %c", &linha, &coluna, &dir);
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
    seuMapa();
    recebeNavio("Encouraçado", 4, 1);
    seuMapa();

    int i = 0;
    do {
        recebeNavio("Cruzador", 3, (2 + i));
        i++;
        seuMapa();
    } while (i < 2);

    i = 0;
    do {
        recebeNavio("Destróier", 2, (4 + i));
        i++;
        seuMapa();
    } while (i < 2);
}

void telaJogo() {
    //system("clear");
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
    // printf("---------------------\n");
    // printf("------Resposta-------\n");
    // printf("  0 1 2 3 4 5 6 7 8 9\n");
    // for (int i = 0; i < TAMANHO_MAXIMO; i++) {
    //     printf("%d ", i);
    //     for (int j = 0; j < TAMANHO_MAXIMO; j++) {
    //         printf("%c ", gradeAdversarioResposta[i][j]);   
    //     }
    //     printf("\n");
    // }
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
    
    // printf("oi");
    // printf("%d\n",l);
    // printf("%d\n",c);
    // printf("%c\n",gradeRes[l][c]);
    if (gradeRes[l][c] == 'N') {
        //printf("OIOI");
        for (int i = 0; i < QUANTIDADE_NAVIOS; i++) {
            Navio *n = &navio[i];
            for (int j = 0; j < n->tamanho; j++) {
                // printf("OLA\n");
                // printf("posicoes %d",n->posicoes[j][0]);
                // printf("%d\n",n->posicoes[j][1]);
                // printf("jogada %d",l);
                // printf("%d\n", c);
                if (n->posicoes[j][0] == l && n->posicoes[j][1] == c) {
                    //printf("entrei no if");
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
        strcpy(erro.mensagem, "Você ganhou o jogo!");
        return erro;
    }
    
    if (contNaviosAbatidosJog == QUANTIDADE_NAVIOS) {
        erro.isSuccess = true;
        strcpy(erro.mensagem, "Você perdeu o jogo!");
        return erro;
    }
    
    erro.isSuccess = false;
    return erro;
}

Erro jogar(int loc_newsockfd) {
    Erro erroGanhou;

    bool controle = true;
    char jogada[3];
    
    printf("Sua vez de jogar...\n");
    do {
        int coluna = -1;
        int linha = -1;
        scanf(" %d %d", &linha, &coluna);
        Erro erro = processaJogada(coluna, linha);
        if (!erro.isSuccess) {
            printf(erro.mensagem);
            printf("\n");
        } else {
            sprintf(jogada, "%d%d", erro.jogada[0], erro.jogada[1]);
            //printf("Dentro do erro %d%d\n",erro.jogada[0],erro.jogada[1]);
            int l = erro.jogada[0];
            int c = erro.jogada[1];
            //printf("variaveis %d%d\n",l,c);
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
            //printf("Jogada %s\n", jogada);
            erroGanhou = validaGanhou(false);
            telaJogo();
            if (erroGanhou.isSuccess) {
                printf(erroGanhou.mensagem);
            } else {
                printf(erro.mensagem);
            }
            printf("\n");
        }
    } while (controle);
   
    send(loc_newsockfd, jogada, strlen(jogada), 0);
    return erroGanhou;
}

Erro receberJogada(int rem_sockfd) {
    //telaJogo();
    printf("Esperando o adversário jogar...\n");
    char jogada[3];
    recv(rem_sockfd, &jogada, sizeof(jogada), 0);
    //printf("Recebi %d %d\n", jogada[0] - '0', jogada[1] - '0');
    //printf("Recebi %s\n", jogada);

    int l = (jogada[0] - '0');
    int c = (jogada[1] - '0');
    //printf("variaveis %d%d\n", l, c);
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
    }
    printf("\n");
    return erroGanhou;
}

void *mainServidor(void *arg) {
    char **argv = (char **)arg;
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

    inicializaNaviosJogador();
    char *json = gerarJSONComCJSON();

    // Envia o JSON para o cliente assim que a conexão é estabelecida
    if (send(loc_newsockfd, json, sizeof(linha), 0) < 0) {
        perror("Erro ao enviar JSON para o cliente");
        close(loc_sockfd);
        close(loc_newsockfd);
        free(json);
        exit(1);
    }

    //printf("JSON enviado para o cliente:\n%s\n", json);

    printf("Esperando adversário posicionar os Navios...\n");
    recv(loc_newsockfd, &linha, sizeof(linha), 0);
    //printf("Recebi: %s\n", linha);
    char *jsonAdv = linha;
    converterJsonParaNavios(jsonAdv);
    marcarNaviosAdversariosPosIncializacao();
    free(json); // Liberar a memória alocada para o JSON

    //telaJogo();
    printf("Você começa jogando...\n");
    printf("Envie a posição em que quer disparar o tiro\n");
    printf("Exemplo 2 2\n");

    do {
        Erro er1 = jogar(loc_newsockfd);
        if (er1.isSuccess) {
            break;
        }
        Erro er2 = receberJogada(loc_newsockfd);
        if (er2.isSuccess) {
            break;
        }
    } while (true);

    close(loc_sockfd);
    close(loc_newsockfd);
}

void* mainCliente(void* arg) {
    char **argv = (char**)arg;
    char *rem_hostname;
    int rem_port;
    /* Estrutura: familia + endereco IP + porta */
    struct sockaddr_in rem_addr;
    int rem_sockfd;

    rem_hostname = argv[1];
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

    printf("Esperando adversário posicionar os Navios...\n");

    char linha[1000];
    recv(rem_sockfd, linha, sizeof(linha), 0);
    //printf("JSON recebido do servidor:\n%s\n", linha);

    char *jsonAdv = linha;
    converterJsonParaNavios(jsonAdv);
    marcarNaviosAdversariosPosIncializacao();

    inicializaNaviosJogador();
    char *json = gerarJSONComCJSON();

    if (send(rem_sockfd, json, strlen(json), 0) < 0) {
        perror("Erro ao enviar JSON para o servidor");
        close(rem_sockfd);
        free(json);
        exit(1);
    }

    free(json);
    do {
        Erro er2 = receberJogada(rem_sockfd);
        if (er2.isSuccess) {
            break;
        }
        Erro er1 = jogar(rem_sockfd);
        if (er1.isSuccess) {
            break;
        }
    } while (true);

    close(rem_sockfd);
}

int main(int argc, char *argv[]) {
    pthread_t tid[4];
    void *statusCli, *statusSer;

    int controleMenu = 0;
    do {
        printf("---------------------\n");
        printf("----Batalha Naval----\n");
        printf("---------------------\n");

        printf("1 - Iniciar a partida\n");
        printf("2 - Entrar em uma partida corrente\n");
        printf("3 - Sair\n");
        scanf("%d", &controleMenu);

        preencheComZerosGrades();
        switch (controleMenu) {
            case 1:
                // Servidor
                if (pthread_create(tid + 1, 0, mainServidor, (void *)argv) != 0) {
                    perror("Erro ao criar thread do servidor....");
                    exit(1);
                }
                if (pthread_join(tid[1], &statusSer) != 0) {
                    perror("Erro no pthread_join() do servidor.");
                    exit(1);
                }
                break;
            case 2:
                // Cliente
                if (pthread_create(tid + 0, 0, mainCliente, (void *)argv) != 0) {
                    perror("Erro ao criar thread do cliente....");
                    exit(1);
                }
                // Esperando as threads terminarem
                if (pthread_join(tid[0], &statusCli) != 0) {
                    perror("Erro no pthread_join() do cliente.");
                    exit(1);
                }
                break;
            case 3:
                printf("Saindo\n");
                printf("Até a próxima!\n");
                break;
            default:
                printf("Opção não permitida!\n");
                printf("Tente novamente\n");
                break;
        }
        
    } while (controleMenu!=3);
    exit(0);
}
