#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

#define MAX_LINHAS 1000
#define MAX_COLUNAS 82
#define MAX_LINHAS_PALAVRA 15
#define MAX_TAM_PALAVRA 20
#define COLUNAS_UTEIS 80
#define MAX_OCORRENCIAS 500
#define NUM_THREADS 4

typedef struct {
    char palavra[MAX_TAM_PALAVRA];
    int linhaIni, colIni;
    int linhaFim, colFim;
} Ocorrencia;

typedef struct {
    char (*matriz)[MAX_COLUNAS];
    char (*arrayPalavras)[MAX_TAM_PALAVRA];
    int palavraInicio, palavraFim; // faixa de palavras para a thread
    int qtdLinhasMatriz;
} ThreadData;

// Vetor global para ocorrências encontradas
Ocorrencia ocorrencias[MAX_OCORRENCIAS];
int contadorOcorrencias = 0;

// Mutex para sincronização
pthread_mutex_t mutexOcorrencias = PTHREAD_MUTEX_INITIALIZER;

// Função de leitura do arquivo palavras.txt
int lerArquivoPalavra(const char *nomeArquivo, char arrayPalavras[MAX_LINHAS_PALAVRA][MAX_TAM_PALAVRA]) {
    FILE *arquivo = fopen(nomeArquivo, "r");
    if (!arquivo) { perror("Erro ao abrir arquivo de palavras"); return -1; }

    int qtd = 0;
    while (fgets(arrayPalavras[qtd], MAX_TAM_PALAVRA, arquivo)) {
        arrayPalavras[qtd][strcspn(arrayPalavras[qtd], "\r\n")] = '\0';
        qtd++;
        if (qtd >= MAX_LINHAS_PALAVRA) break;
    }
    fclose(arquivo);
    return qtd;
}

// Função de leitura do arquivo cacapalavras.txt
int lerCacaPalavras(const char *nomeArquivo, char matriz[MAX_LINHAS][MAX_COLUNAS]) {
    FILE *arquivo = fopen(nomeArquivo, "r");
    if (!arquivo) { perror("Erro ao abrir caça-palavras"); return 0; }

    char linha[MAX_COLUNAS];
    int i = 0;
    while (fgets(linha, sizeof(linha), arquivo)) {
        linha[strcspn(linha, "\r\n")] = '\0';
        strcpy(matriz[i], linha);
        i++;
        if (i >= MAX_LINHAS) break;
    }
    fclose(arquivo);
    return i;
}

// Função que cada thread executa para procurar palavras dentro da matriz
void* procuraThread(void* arg) {
    ThreadData *data = (ThreadData*)arg;
    // Direções de busca horizontal, vertical, diagonal  
    int direcoes[8][2] = { {0,1},{0,-1},{1,0},{-1,0},{1,1},{1,-1},{-1,1},{-1,-1} };
    // Percorre as palavras atribuídas a esta thread e armazena na variavel 'palavra'
    for (int p = data->palavraInicio; p < data->palavraFim; p++) {
        char *palavra = data->arrayPalavras[p];
        int tamanho = strlen(palavra); // tamanho da palavra
        if (tamanho == 0) continue; // se a palavra estiver vazia, pula
        // percorre a matriz
        for (int linha = 0; linha < data->qtdLinhasMatriz; linha++) {
            for (int coluna = 0; coluna < COLUNAS_UTEIS; coluna++) {
                if (tolower(data->matriz[linha][coluna]) != tolower(palavra[0])) continue; // se a primeira letra não bater com a primeira letra da palavra, pula
                // d = direção (0 a 7), 
                for (int d = 0; d < 8; d++) {
                    int dx = direcoes[d][0], dy = direcoes[d][1]; //exemplo: dx = 0 dy = 1 (horizontal direita)
                    int k; // k é o índice da letra na palavra
                    // verifica se a palavra cabe na direção escolhida
                    for (k = 1; k < tamanho; k++) {
                        int novaLinha = linha + k*dx; // novaLinha = linha + k*0 = linha ou seja, permanece na mesma linha
                        int novaColuna = coluna + k*dy; // novaColuna = coluna + k*1 = coluna + k (anda para a direita)
                        // verifica limites da matriz e se a letra bate
                        if (novaLinha < 0 || novaLinha >= data->qtdLinhasMatriz || 
                            novaColuna < 0 || novaColuna >= COLUNAS_UTEIS) break;
                        if (tolower(data->matriz[novaLinha][novaColuna]) != tolower(palavra[k])) break; // compara letra a letra
                    }
                    // se encontrou a palavra inteira
                    if (k == tamanho) {
                        // trava o mutex para atualizar ocorrências, sem isso pode dar conflito entre threads ja que pode causar escrita simultânea
                        pthread_mutex_lock(&mutexOcorrencias);
                        if (contadorOcorrencias < MAX_OCORRENCIAS) {
                            strcpy(ocorrencias[contadorOcorrencias].palavra, palavra);
                            ocorrencias[contadorOcorrencias].linhaIni = linha;
                            ocorrencias[contadorOcorrencias].colIni = coluna;
                            ocorrencias[contadorOcorrencias].linhaFim = linha + (tamanho-1)*dx; // calcula a linha final
                            ocorrencias[contadorOcorrencias].colFim = coluna + (tamanho-1)*dy; // calcula a coluna final
                            contadorOcorrencias++;
                        }
                        pthread_mutex_unlock(&mutexOcorrencias); // libera o mutex
                    }
                }
            }
        }
    }
}

// Função para gerar arquivos de saída
void gerarArquivosSaida(char matriz[MAX_LINHAS][MAX_COLUNAS], int qtdLinhasMatriz) {

    FILE *relatorio = fopen("resultado.txt", "w"); // abre o arquivo para escrita
    if (!relatorio) { perror("Erro ao criar resultado.txt"); return; } // verifica se abriu corretamente

    fprintf(relatorio, "Palavras encontradas: %d\n\n", contadorOcorrencias); // escreve no arquivo a quantidade de palavras encontradas
    // escreve cada ocorrência
    for (int i = 0; i < contadorOcorrencias; i++) {
        fprintf(relatorio, "%s: inicio=(%d,%d) fim=(%d,%d)\n", // formato de saída
                ocorrencias[i].palavra,
                ocorrencias[i].linhaIni, ocorrencias[i].colIni,
                ocorrencias[i].linhaFim, ocorrencias[i].colFim);
    }
    fclose(relatorio);

    //Cópia do caça-palavras com destaque
    FILE *saida = fopen("caca_saida.txt", "w");
    if (!saida) { perror("Erro ao criar caca_saida.txt"); return; }

    char copia[MAX_LINHAS][COLUNAS_UTEIS];
    // copia matriz original
    for (int i = 0; i < qtdLinhasMatriz; i++)
        for (int j = 0; j < COLUNAS_UTEIS; j++)
            copia[i][j] = matriz[i][j];

    // marca ocorrências
    for (int o = 0; o < contadorOcorrencias; o++) {
        int li = ocorrencias[o].linhaIni, ci = ocorrencias[o].colIni; // linha inicial e coluna inicial
        int lf = ocorrencias[o].linhaFim, cf = ocorrencias[o].colFim; // linha final e coluna final 
        int dx = (lf - li == 0) ? 0 : ((lf - li) / abs(lf - li)); // direção x (0, 1 ou -1)
        int dy = (cf - ci == 0) ? 0 : ((cf - ci) / abs(cf - ci)); // direção y (0, 1 ou -1)
        int len = strlen(ocorrencias[o].palavra);
        // destaca a palavra na cópia
        for (int k = 0; k < len; k++)
            copia[li + k*dx][ci + k*dy] = toupper(copia[li + k*dx][ci + k*dy]); 
    }

    // escreve no arquivo de saída, destacando letras maiúsculas com asteriscos
    for (int i = 0; i < qtdLinhasMatriz; i++) {
        for (int j = 0; j < COLUNAS_UTEIS; j++) {
            if (isupper(copia[i][j])) fprintf(saida, "*%c*", copia[i][j]);
            else fputc(copia[i][j], saida);
        }
        fputc('\n', saida);
    }
    fclose(saida);
}

int main() {
    // instancia a matriz e o array de palavras
    char matriz[MAX_LINHAS][MAX_COLUNAS];
    char arrayPalavras[MAX_LINHAS_PALAVRA][MAX_TAM_PALAVRA];
    // le e armazenar os dados dos arquivos
    int qtdLinhasMatriz = lerCacaPalavras("texts/cacapalavras.txt", matriz);
    int qtdPalavras = lerArquivoPalavra("texts/palavras.txt", arrayPalavras);
    // cria threads
    pthread_t threads[NUM_THREADS];
    ThreadData tdata[NUM_THREADS];

    // divide palavras entre threads
    int base = qtdPalavras / NUM_THREADS; // base = 3 palavras por thread
    int resto = qtdPalavras % NUM_THREADS; // caso o numero de palavras não seja múltiplo do número de threads
    int inicio = 0;

    for (int t = 0; t < NUM_THREADS; t++) {
        tdata[t].matriz = matriz;
        tdata[t].arrayPalavras = arrayPalavras;
        tdata[t].qtdLinhasMatriz = qtdLinhasMatriz;
        tdata[t].palavraInicio = inicio;
        tdata[t].palavraFim = inicio + base + (t < resto ? 1 : 0); // (t < resto ? 1 : 0) distribui o resto nas primeiras threads
        inicio = tdata[t].palavraFim; // atualiza o início para a próxima thread
        pthread_create(&threads[t], NULL, procuraThread, &tdata[t]); // cria a thread e roda a função procuraThread
    }

    // espera threads terminarem, para não gerar arquivos antes de terminar a busca
    for (int t = 0; t < NUM_THREADS; t++)
        pthread_join(threads[t], NULL);

    printf("Total de palavras encontradas: %d\n", contadorOcorrencias);

    gerarArquivosSaida(matriz, qtdLinhasMatriz);
    printf("Arquivos 'resultado.txt' e 'caca_saida.txt' gerados!\n");

    pthread_mutex_destroy(&mutexOcorrencias); // destrói o mutex
    return 0;
}
