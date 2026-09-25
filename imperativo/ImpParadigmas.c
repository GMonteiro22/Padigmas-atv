#include <stdio.h>
#include <stdlib.h>

/* ============================================================
 *  VARIAVEIS GLOBAIS
 * ============================================================ */

int tabuleiro[9][9];

/* ============================================================
 *  ESTRUTURAS
 * ============================================================ */

// Lista encadeada usada para registrar conflitos encontrados na validacao
typedef struct Noerro
{
    int numero;
    int linha1;
    int coluna1;
    int linha2;
    int coluna2;
    struct Noerro *prox;
} NoE;

// Lista encadeada usada para agrupar as posicoes de um mesmo digito
typedef struct NoDig
{
    int numero;
    int linha;
    int coluna;
    struct NoDig *prox;
} NoD;

// Estado dos bitmasks usados durante a resolucao (backtracking).
// Agrupar os tres vetores numa struct e passar por ponteiro, em vez de
// usar variaveis globais, deixa explicito nos parametros de cada funcao
// qual estado ela le/modifica - o fluxo de dados fica visivel na
// assinatura, e nao escondido em variaveis externas.
typedef struct
{
    unsigned int linha[9];   // linha[i]  = digitos ja usados na linha i
    unsigned int coluna[9];  // coluna[j] = digitos ja usados na coluna j
    unsigned int bloco[9];   // bloco[b]  = digitos ja usados no bloco b (0 a 8)
} EstadoBitmask;


/* ============================================================
 *  FUNCOES AUXILIARES - LISTA DE DIGITOS (NoD)
 * ============================================================ */

NoD* CriarNoDigTab(int n, int l, int c) {
    NoD *novo = (NoD*) malloc(sizeof(NoD));
    if (novo == NULL) {
        printf("Erro: Memoria insuficiente\n");
        exit(1);
    }
    novo->numero = n;
    novo->linha = l;
    novo->coluna = c;
    novo->prox = NULL;
    return novo;
}

void InserirFimDigTab(NoD **head, int n, int l, int c) {
    NoD *novo = CriarNoDigTab(n, l, c);
    if (*head == NULL) {
        *head = novo;
        return;
    }
    NoD *atual = *head;
    while (atual->prox != NULL) {
        atual = atual->prox;
    }
    atual->prox = novo;
}

void FreelistDigTab(NoD **head) {
    NoD *atual = *head;
    NoD *proximo;
    while (atual != NULL) {
        proximo = atual->prox;
        free(atual);
        atual = proximo;
    }
    *head = NULL;
}


/* ============================================================
 *  FUNCOES AUXILIARES - LISTA DE ERROS (NoE)
 * ============================================================ */

NoE* CriarNoerrosTab(int n, int l1, int c1, int l2, int c2) {
    NoE *novo = (NoE*) malloc(sizeof(NoE));
    if (novo == NULL) {
        printf("Erro: Memoria insuficiente\n");
        exit(1);
    }
    novo->numero = n;
    novo->linha1 = l1;
    novo->coluna1 = c1;
    novo->linha2 = l2;
    novo->coluna2 = c2;
    novo->prox = NULL;
    return novo;
}

void InserirFimErrosTab(NoE **head, int n, int l1, int c1, int l2, int c2) {
    NoE *novo = CriarNoerrosTab(n, l1, c1, l2, c2);
    if (*head == NULL) {
        *head = novo;
        return;
    }
    NoE *atual = *head;
    while (atual->prox != NULL) {
        atual = atual->prox;
    }
    atual->prox = novo;
}

void FreelistErroTab(NoE **head) {
    NoE *atual = *head;
    NoE *proximo;
    while (atual != NULL) {
        proximo = atual->prox;
        free(atual);
        atual = proximo;
    }
    *head = NULL;
}

void PrintErrosTab(NoE *head) {
    printf("Os erros encontrados no tabuleiro foram:\n");
    NoE *atual = head;
    while (atual != NULL) {
        printf("O digito %d nos pontos [%d,%d] - [%d,%d]\n",
               atual->numero, atual->linha1 + 1, atual->coluna1 + 1,
               atual->linha2 + 1, atual->coluna2 + 1);
        atual = atual->prox;
    }
    printf("/\n");
}


/* ============================================================
 *  FUNCOES DE RESOLUCAO (backtracking + MRV + bitmask)
 * ============================================================ */

// Descobre o indice do bloco 3x3 (0 a 8) a partir de linha/coluna.
// Dividir por 3 "arredonda para baixo" ate o inicio do bloco (0, 3 ou 6),
// entao a formula (linha/3)*3 + (coluna/3) numera os 9 blocos de 0 a 8,
// varrendo da esquerda para a direita e de cima para baixo.
int indiceBloco(int linha, int coluna) {
    return (linha / 3) * 3 + (coluna / 3);
}

// Monta os tres bitmasks a partir do tabuleiro ja lido do arquivo.
// Cada bit representa um digito (bit 0 = digito 1, bit 8 = digito 9);
// ligamos o bit correspondente em linha/coluna/bloco para cada pista
// ja preenchida, de forma que consultas futuras sejam O(1) em vez de
// percorrer a linha/coluna/bloco inteiros a cada tentativa.
void inicializarBitmasks(EstadoBitmask *estado, int m[9][9]) {
    for (int i = 0; i < 9; i++) {
        estado->linha[i] = 0;
        estado->coluna[i] = 0;
        estado->bloco[i] = 0;
    }

    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            if (m[i][j] != 0) {
                int valor = m[i][j];
                int b = indiceBloco(i, j);

                estado->linha[i]  |= (1 << (valor - 1));
                estado->coluna[j] |= (1 << (valor - 1));
                estado->bloco[b]  |= (1 << (valor - 1));
            }
        }
    }
}

// Retorna um bitmask com os digitos ainda disponiveis para uma celula.
// OR entre os tres bitmasks reune tudo que ja esta ocupado em qualquer
// uma das tres unidades (linha OU coluna OU bloco); o NOT inverte para
// obter o que ainda esta livre, e o AND com 0x1FF (9 bits em 1) descarta
// os bits acima do 9o, que nao tem significado no nosso dominio.
unsigned int obterCandidatos(EstadoBitmask *estado, int linha, int coluna) {
    int b = indiceBloco(linha, coluna);
    unsigned int ocupados = estado->linha[linha] | estado->coluna[coluna] | estado->bloco[b];
    unsigned int candidatos = (~ocupados) & 0x1FF;
    return candidatos;
}

// Conta quantos bits (candidatos) estao ligados em um bitmask
int contarBits(unsigned int x) {
    int contagem = 0;
    while (x) {
        contagem += (x & 1);
        x >>= 1;
    }
    return contagem;
}

// Coloca um valor na celula e atualiza os tres bitmasks correspondentes
void colocarValor(EstadoBitmask *estado, int linha, int coluna, int valor, int m[9][9]) {
    int b = indiceBloco(linha, coluna);
    m[linha][coluna] = valor;
    estado->linha[linha]  |= (1 << (valor - 1));
    estado->coluna[coluna] |= (1 << (valor - 1));
    estado->bloco[b]       |= (1 << (valor - 1));
}

// Remove um valor da celula e desfaz a atualizacao dos bitmasks (backtrack).
// O NOT do bit desligado (~(1<<(valor-1))) cria uma mascara com todos os
// bits em 1, exceto o do valor removido; o AND preserva os demais bits e
// zera apenas aquele, sem afetar outros digitos que continuam em uso.
void removerValor(EstadoBitmask *estado, int linha, int coluna, int valor, int m[9][9]) {
    int b = indiceBloco(linha, coluna);
    m[linha][coluna] = 0;
    estado->linha[linha]  &= ~(1 << (valor - 1));
    estado->coluna[coluna] &= ~(1 << (valor - 1));
    estado->bloco[b]       &= ~(1 << (valor - 1));
}

// Heuristica MRV (Minimum Remaining Values): em vez de resolver as celulas
// em ordem fixa, escolhemos sempre a celula vazia com MENOS candidatos
// possiveis. Isso reduz drasticamente o fator de ramificacao da busca,
// pois celulas quase decididas (1 ou 2 candidatos) eliminam ambiguidade
// cedo, evitando explorar ramos que fracassariam de qualquer forma mais
// adiante na recursao.
int escolherCelula(EstadoBitmask *estado, int m[9][9], int *linhaEscolhida, int *colunaEscolhida) {
    int menorContagem = 10; // maior que o maximo possivel (9), forca a 1a comparacao a "ganhar"
    int encontrou = 0;

    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            if (m[i][j] == 0) {
                int qtd = contarBits(obterCandidatos(estado, i, j));
                if (qtd < menorContagem) {
                    menorContagem = qtd;
                    *linhaEscolhida = i;
                    *colunaEscolhida = j;
                    encontrou = 1;
                }
            }
        }
    }

    return encontrou; // 0 se nao sobrou nenhuma celula vazia
}

// Funcao recursiva principal de resolucao (backtracking).
// A cada chamada: escolhe a celula mais restrita (MRV), tenta cada
// candidato possivel, e recua (backtrack) se a tentativa nao levar a
// uma solucao completa. A poda antecipada (candidatos == 0) evita
// continuar preenchendo um tabuleiro que ja sabemos que vai falhar.
int resolverSudoku(EstadoBitmask *estado, int m[9][9]) {
    int linha, coluna;

    if (!escolherCelula(estado, m, &linha, &coluna)) {
        return 1; // nao sobrou celula vazia -> resolvido!
    }

    unsigned int candidatos = obterCandidatos(estado, linha, coluna);

    if (candidatos == 0) {
        return 0; // celula sem nenhum candidato -> esse caminho nao funciona
    }

    for (int valor = 1; valor <= 9; valor++) {
        if (candidatos & (1 << (valor - 1))) {
            colocarValor(estado, linha, coluna, valor, m);

            if (resolverSudoku(estado, m)) {
                return 1;
            }

            removerValor(estado, linha, coluna, valor, m); // backtrack
        }
    }

    return 0; // nenhum candidato funcionou -> falha, volta um nivel
}

// Imprime o tabuleiro (usado apos a resolucao)
void imprimirTabuleiro(int m[9][9]) {
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            if ((j+1)%3==0)
            {
                printf("%d |", m[i][j]);
            }
            else {
            printf("%d ", m[i][j]);
            }
        }
        if ((i+1)%3==0)
        {
            printf("\n---------------------\n");
        }
        else {
        printf("\n");
        }
    }
}


/* ============================================================
 *  FUNCOES PRINCIPAIS (leitura de arquivo e validacao)
 * ============================================================ */

int lerTabuleiro(const char *caminhoArquivo, int tab[9][9]) {
    FILE *arquivo = fopen(caminhoArquivo, "r");
    if (arquivo == NULL) {
        printf("Erro: nao foi possivel abrir o arquivo.\n");
        return 0;
    }

    char linha[16]; // margem de seguranca para \n, \r, etc.

    for (int i = 0; i < 9; i++) {
        if (fgets(linha, sizeof(linha), arquivo) == NULL) {
            printf("Erro: arquivo com menos de 9 linhas.\n");
            fclose(arquivo);
            return 0;
        }
        for (int j = 0; j < 9; j++) {
            char c = linha[j];
            if (c == '0') {
                tab[i][j] = 0; // celula vazia
            } else if (c >= '1' && c <= '9') {
                tab[i][j] = c - '0';
            } else {
                printf("Erro: caractere invalido na linha %d.\n", i + 1);
                fclose(arquivo);
                return 0;
            }
        }
    }

    fclose(arquivo);
    return 1;
}

// Valida um tabuleiro contra as tres regras do Sudoku (linha, coluna e
// bloco). Para cada digito de 1 a 9, agrupamos todas as posicoes onde
// ele aparece (lista Qtddig) e comparamos cada par de posicoes entre si:
// se compartilharem linha, coluna ou bloco, ha um conflito, registrado
// na lista de erros (ListErro).
int ValTabuleiro(int m[9][9], NoE **ListErro) {
    NoD *Qtddig;
    int SameL, SameC, SameB;
    int totalErros = 0;

    for (int digito = 1; digito <= 9; digito++) {
        Qtddig = NULL;

        // Monta a lista de todas as posicoes onde esse digito aparece
        for (int j = 0; j < 9; j++) {
            for (int k = 0; k < 9; k++) {
                if (m[j][k] == digito) {
                    InserirFimDigTab(&Qtddig, digito, j, k);
                }
            }
        }

        // Compara cada posicao encontrada com todas as posteriores
        // (comecar "aux" em "a->prox" evita comparar uma celula com ela
        // mesma e evita reportar o mesmo par de erro duas vezes)
        NoD *a = Qtddig;
        while (a != NULL) {
            NoD *aux = a->prox;
            while (aux != NULL) {
                SameL = (a->linha == aux->linha);
                SameC = (a->coluna == aux->coluna);
                SameB = (a->linha / 3 == aux->linha / 3) && (a->coluna / 3 == aux->coluna / 3);

                if (SameL || SameC || SameB) {
                    InserirFimErrosTab(ListErro, digito, a->linha, a->coluna, aux->linha, aux->coluna);
                    totalErros++;
                }

                aux = aux->prox;
            }
            a = a->prox;
        }

        FreelistDigTab(&Qtddig); // libera a lista antes do proximo digito
    }

    return totalErros; // 0 = tabuleiro valido, >0 = quantidade de conflitos encontrados
}


/* ============================================================
 *  FUNCAO PRINCIPAL (main)
 * ============================================================ */

int main() {
    int dec;
// le o tabuleiro no arquivo .txt e o insere na matriz 9x9
    if (!lerTabuleiro("tabuleiro.txt", tabuleiro)) {
        return 1;
    }
/* o usuário digita uma das três ações possíveis:
1- validar tabuleiro (verifica se o tabuleiro ainda não preenchido cumpre com as regras do jogo Sudoku);
2- resolver tabuleiro (resolve o tabuleiro Sudoku fornecido no arquivo "tabuleiro.txt");
3- valida resposta (verifica se o tabuleiro em "tabuleiro.txt" e "tabuleiro2.txt" possuem os mesmos valores fixos e depois verifica se a resposta é válida) */
    printf("Digite a seguir qual acao queira fazer:\n1-Validar tabuleiro\n2-Resolver tabuleiro\n3-Validar resposta(insira o tabuleiro resolvido no arquivo \"tabuleiro2.txt\")\nDigite aqui:");
    scanf("%d", &dec);
// loop que irá funcionar enquanto a variável "dec" não recebe algum valor aceitável das tres possíveis ações
    while (dec != 1 && dec != 2 && dec != 3) {
        printf("Acao nao encontrada, tente novamente:");
        scanf("%d", &dec);
    }

    switch (dec) {
        case 1: // Validação de tabuleiro
        {
            NoE *listaErros = NULL;
            int erros = ValTabuleiro(tabuleiro, &listaErros);

            if (erros == 0) {
                printf("Tabuleiro valido!\n");
            } else {
                PrintErrosTab(listaErros);
            }

            FreelistErroTab(&listaErros);
            break;
        }

        case 2: // Resolução de tabuleiro
        {
            /* Antes de tentar resolver, garante que o tabuleiro inicial é válido
             rodar o backtracking sobre um tabuleiro já inconsistente nao faz sentido.*/
            NoE *listaErros = NULL;
            int erros = ValTabuleiro(tabuleiro, &listaErros);

            if (erros > 0) {
                printf("Tabuleiro invalido, nao e possivel resolver:\n");
                PrintErrosTab(listaErros);
                FreelistErroTab(&listaErros);
                break;
            }
            FreelistErroTab(&listaErros);

            EstadoBitmask estado;
            inicializarBitmasks(&estado, tabuleiro);

            if (resolverSudoku(&estado, tabuleiro)) {
                printf("Solucao encontrada:\n");
                imprimirTabuleiro(tabuleiro);
            } else {
                printf("Este tabuleiro nao possui solucao.\n");
            }

            break;
        }

        case 3: // Validação de resposta
        {
            int tabuleiroResposta[9][9];

            if (!lerTabuleiro("tabuleiro2.txt", tabuleiroResposta)) {
                break;
            }

            // Verifica se o tabuleiro está completo (nenhuma célula vazia)
            int completo = 1;
            for (int i = 0; i < 9 && completo; i++) {
                for (int j = 0; j < 9; j++) {
                    if (tabuleiroResposta[i][j] == 0) {
                        completo = 0;
                        break;
                    }
                }
            }

            /* Verifica se a resposta respeita as pistas do tabuleiro original
             (uma solução "sem conflitos" não é válida se alterou uma pista fixa) */
            int respeitaPistas = 1;
            for (int i = 0; i < 9 && respeitaPistas; i++) {
                for (int j = 0; j < 9; j++) {
                    if (tabuleiro[i][j] != 0 && tabuleiro[i][j] != tabuleiroResposta[i][j]) {
                        respeitaPistas = 0;
                        break;
                    }
                }
            }

            // Verifica infrações de regras (linha, coluna, bloco)
            NoE *listaErros = NULL;
            int erros = ValTabuleiro(tabuleiroResposta, &listaErros);

            if (!completo) {
                printf("Resposta invalida: existem celulas vazias.\n");
            } else if (!respeitaPistas) {
                printf("Resposta invalida: nao respeita os valores originais do tabuleiro.\n");
            } else if (erros > 0) {
                printf("Resposta invalida: ha conflitos.\n");
                PrintErrosTab(listaErros);
            } else {
                printf("Resposta valida! Parabens.\n");
            }

            FreelistErroTab(&listaErros);
            break;
        }
    }

    return 0; // fim da execução do programa
}