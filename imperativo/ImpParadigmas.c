#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
VARIÁVEIS GLOBAIS
============================================================ */

int tabuleiro[9][9]; // [ESTADO] Estado central do programa: conteúdo do Sudoku, modificado por lerTabuleiro e pelo backtracking.

/* ============================================================
ESTRUTURAS
============================================================ */

// Lista encadeada usada para registrar conflitos encontrados na validação.
typedef struct Noerro
{
    int numero;
    int linha1;
    int coluna1;
    int linha2;
    int coluna2;
    struct Noerro *prox;
} NoE;

// Lista encadeada usada para agrupar as posições de um mesmo dígito.
typedef struct NoDig
{
    int numero;
    int linha;
    int coluna;
    struct NoDig *prox;
} NoD;

/*
Estado dos bitmasks usados durante a resolução (backtracking).
Agrupar os três vetores numa struct e passar por ponteiro, em vez de
usar variáveis globais, deixa explícito nos parâmetros de cada função
qual estado ela lê/modifica - o fluxo de dados fica visível na
assinatura, e não escondido em variáveis externas.
*/
typedef struct // [ESTADO] Estado auxiliar temporário, existe apenas durante a resolução (case 2).
{
    unsigned int linha[9];   // linha[i]  = dígitos já usados na linha i.
    unsigned int coluna[9];  // coluna[j] = dígitos já usados na coluna j.
    unsigned int bloco[9];   // bloco[b]  = dígitos já usados no bloco b (0 a 8).
} EstadoBitmask;


/* ============================================================
FUNÇÕES AUXILIARES - LISTA DE DÍGITOS (NoD)
============================================================ */

// Cria e inicializa um novo nó da lista de dígitos, alocando memória dinamicamente.
NoD* CriarNoDigTab(int n, int l, int c) {
    NoD *novo = (NoD*) malloc(sizeof(NoD)); // [EFEITO COLATERAL] Aloca memória dinâmica (heap).
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

// Insere um novo nó ao final da lista de dígitos apontada por head.
void InserirFimDigTab(NoD **head, int n, int l, int c) { // [EFEITO COLATERAL] Modifica a lista apontada por head (fora do escopo local da função).
    NoD *novo = CriarNoDigTab(n, l, c);
    if (*head == NULL) {
        *head = novo; // [ESTADO] Altera o estado da lista (primeiro nó).
        return;
    }
    NoD *atual = *head;
    while (atual->prox != NULL) { // [CONTROLE] While: avança até o último nó da lista (repetição condicional, sem contagem fixa).
        atual = atual->prox;
    }
    atual->prox = novo; // [ESTADO] Altera o estado da lista (encadeia o novo nó no final).
}

// Libera toda a memória alocada pela lista de dígitos e zera o ponteiro do chamador.
void FreelistDigTab(NoD **head) { // [EFEITO COLATERAL] Desaloca toda a lista e zera o ponteiro do chamador.
    NoD *atual = *head;
    NoD *proximo;
    while (atual != NULL) { // [CONTROLE] While: percorre e libera cada nó até o fim da lista.
        proximo = atual->prox;
        free(atual); // [EFEITO COLATERAL] Libera memória dinâmica.
        atual = proximo;
    }
    *head = NULL; // [ESTADO] Lista volta ao estado "vazia".
}


/* ============================================================
FUNÇÕES AUXILIARES - LISTA DE ERROS (NoE)
============================================================ */

// Cria e inicializa um novo nó da lista de erros, alocando memória dinamicamente.
NoE* CriarNoerrosTab(int n, int l1, int c1, int l2, int c2) {
    NoE *novo = (NoE*) malloc(sizeof(NoE)); // [EFEITO COLATERAL] Aloca memória dinâmica (heap).
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

// Insere um novo nó ao final da lista de erros apontada por head.
void InserirFimErrosTab(NoE **head, int n, int l1, int c1, int l2, int c2) { // [EFEITO COLATERAL] Modifica a lista de erros apontada por head.
    NoE *novo = CriarNoerrosTab(n, l1, c1, l2, c2);
    if (*head == NULL) {
        *head = novo; // [ESTADO] Altera o estado da lista (primeiro erro registrado).
        return;
    }
    NoE *atual = *head;
    while (atual->prox != NULL) { // [CONTROLE] While: avança até o último nó da lista.
        atual = atual->prox;
    }
    atual->prox = novo; // [ESTADO] Altera o estado da lista (novo erro encadeado no final).
}

// Libera toda a memória alocada pela lista de erros e zera o ponteiro do chamador.
void FreelistErroTab(NoE **head) { // [EFEITO COLATERAL] Desaloca toda a lista de erros e zera o ponteiro do chamador.
    NoE *atual = *head;
    NoE *proximo;
    while (atual != NULL) { // [CONTROLE] While: percorre e libera cada nó até o fim da lista.
        proximo = atual->prox;
        free(atual); // [EFEITO COLATERAL] Libera memória dinâmica.
        atual = proximo;
    }
    *head = NULL; // [ESTADO] Lista volta ao estado "vazia".
}

// Imprime, na saída padrão, todos os erros registrados na lista de erros.
void PrintErrosTab(NoE *head) { // [EFEITO COLATERAL] Escreve na saída padrão (printf).
    printf("Os erros encontrados no tabuleiro foram:\n");
    NoE *atual = head;
    while (atual != NULL) { // [CONTROLE] While: percorre a lista inteira até o fim, imprimindo cada erro.
        printf("O digito %d nos pontos [%d,%d] - [%d,%d]\n",
               atual->numero, atual->linha1 + 1, atual->coluna1 + 1,
               atual->linha2 + 1, atual->coluna2 + 1);
        atual = atual->prox;
    }
    printf("/\n");
}


/* ============================================================
FUNÇÕES DE RESOLUÇÃO (backtracking + MRV + bitmask)
============================================================ */

/*
Descobre o índice do bloco 3x3 (0 a 8) a partir de linha/coluna.
Dividir por 3 "arredonda para baixo" até o início do bloco (0, 3 ou 6),
então a fórmula (linha/3)*3 + (coluna/3) numera os 9 blocos de 0 a 8,
varrendo da esquerda para a direita e de cima para baixo.
*/
int indiceBloco(int linha, int coluna) { // Sem efeito colateral: só calcula e devolve um valor.
    return (linha / 3) * 3 + (coluna / 3);
}

/*
Monta os três bitmasks a partir do tabuleiro já lido do arquivo.
Cada bit representa um dígito (bit 0 = dígito 1, bit 8 = dígito 9);
ligamos o bit correspondente em linha/coluna/bloco para cada pista
já preenchida, de forma que consultas futuras sejam O(1) em vez de
percorrer a linha/coluna/bloco inteiros a cada tentativa.
*/
void inicializarBitmasks(EstadoBitmask *estado, int m[9][9]) { // [EFEITO COLATERAL] Escreve em *estado (recebido por ponteiro).
    for (int i = 0; i < 9; i++) { // [CONTROLE] For: zera os três vetores (contagem fixa, 0 a 8).
        estado->linha[i] = 0;
        estado->coluna[i] = 0;
        estado->bloco[i] = 0;
    }

    for (int i = 0; i < 9; i++) { // [CONTROLE] For aninhado: percorre as 81 células do tabuleiro.
        for (int j = 0; j < 9; j++) {
            if (m[i][j] != 0) {
                int valor = m[i][j];
                int b = indiceBloco(i, j);

                estado->linha[i]  |= (1 << (valor - 1)); // [ESTADO] Liga o bit do dígito na linha i.
                estado->coluna[j] |= (1 << (valor - 1)); // [ESTADO] Liga o bit do dígito na coluna j.
                estado->bloco[b]  |= (1 << (valor - 1)); // [ESTADO] Liga o bit do dígito no bloco b.
            }
        }
    }
}

/*
Retorna um bitmask com os dígitos ainda disponíveis para uma célula.
OR entre os três bitmasks reúne tudo que já está ocupado em qualquer
uma das três unidades (linha OU coluna OU bloco); o NOT inverte para
obter o que ainda está livre, e o AND com 0x1FF (9 bits em 1) descarta
os bits acima do 9º, que não têm significado no nosso domínio.
*/
unsigned int obterCandidatos(EstadoBitmask *estado, int linha, int coluna) { // Sem efeito colateral: apenas lê *estado e devolve um valor.
    int b = indiceBloco(linha, coluna);
    unsigned int ocupados = estado->linha[linha] | estado->coluna[coluna] | estado->bloco[b];
    unsigned int candidatos = (~ocupados) & 0x1FF;
    return candidatos;
}

// Conta quantos bits (candidatos) estão ligados em um bitmask.
int contarBits(unsigned int x) { // Sem efeito colateral: apenas calcula e devolve um valor.
    int contagem = 0;
    while (x) { // [CONTROLE] While: repete enquanto ainda houver bits ligados em x (número de repetições varia por chamada).
        contagem += (x & 1);
        x >>= 1;
    }
    return contagem;
}

// Coloca um valor na célula e atualiza os três bitmasks correspondentes.
void colocarValor(EstadoBitmask *estado, int linha, int coluna, int valor, int m[9][9]) { // [EFEITO COLATERAL] Escreve em *estado e em m (ambos recebidos por ponteiro/referência).
    int b = indiceBloco(linha, coluna);
    m[linha][coluna] = valor; // [ESTADO] Altera o tabuleiro (preenche a célula).
    estado->linha[linha]  |= (1 << (valor - 1)); // [ESTADO] Marca o dígito como usado na linha.
    estado->coluna[coluna] |= (1 << (valor - 1)); // [ESTADO] Marca o dígito como usado na coluna.
    estado->bloco[b]       |= (1 << (valor - 1)); // [ESTADO] Marca o dígito como usado no bloco.
}

/*
Remove um valor da célula e desfaz a atualização dos bitmasks (backtrack).
O NOT do bit desligado (~(1<<(valor-1))) cria uma máscara com todos os
bits em 1, exceto o do valor removido; o AND preserva os demais bits e
zera apenas aquele, sem afetar outros dígitos que continuam em uso.
*/
void removerValor(EstadoBitmask *estado, int linha, int coluna, int valor, int m[9][9]) { // [EFEITO COLATERAL] Desfaz as alterações de colocarValor em *estado e em m.
    int b = indiceBloco(linha, coluna);
    m[linha][coluna] = 0; // [ESTADO] Volta a célula ao estado "vazia" (desfaz a tentativa).
    estado->linha[linha]  &= ~(1 << (valor - 1)); // [ESTADO] Desliga o bit do dígito na linha.
    estado->coluna[coluna] &= ~(1 << (valor - 1)); // [ESTADO] Desliga o bit do dígito na coluna.
    estado->bloco[b]       &= ~(1 << (valor - 1)); // [ESTADO] Desliga o bit do dígito no bloco.
}

/*
Heurística MRV (Minimum Remaining Values): em vez de resolver as células
em ordem fixa, escolhemos sempre a célula vazia com MENOS candidatos
possíveis. Isso reduz drasticamente o fator de ramificação da busca,
pois células quase decididas (1 ou 2 candidatos) eliminam ambiguidade
cedo, evitando explorar ramos que fracassariam de qualquer forma mais
adiante na recursão.
*/
int escolherCelula(EstadoBitmask *estado, int m[9][9], int *linhaEscolhida, int *colunaEscolhida) { // [EFEITO COLATERAL] Escreve o resultado em *linhaEscolhida/*colunaEscolhida (saída via ponteiro).
    int menorContagem = 10; // Maior que o máximo possível (9), força a 1ª comparação a "ganhar".
    int encontrou = 0;

    for (int i = 0; i < 9; i++) { // [CONTROLE] For aninhado: varre TODAS as 81 células para achar a mais restrita (busca completa, não para no primeiro achado).
        for (int j = 0; j < 9; j++) {
            if (m[i][j] == 0) {
                int qtd = contarBits(obterCandidatos(estado, i, j));
                if (qtd < menorContagem) {
                    menorContagem = qtd;
                    *linhaEscolhida = i; // [ESTADO] Atualiza a escolha atual (candidata a melhor célula).
                    *colunaEscolhida = j;
                    encontrou = 1;
                }
            }
        }
    }

    return encontrou; // 0 se não sobrou nenhuma célula vazia.
}

/*
Função recursiva principal de resolução (backtracking).
A cada chamada: escolhe a célula mais restrita (MRV), tenta cada
candidato possível, e recua (backtrack) se a tentativa não levar a
uma solução completa. A poda antecipada (candidatos == 0) evita
continuar preenchendo um tabuleiro que já sabemos que vai falhar.
*/
int resolverSudoku(EstadoBitmask *estado, int m[9][9]) {
    int linha, coluna;

    if (!escolherCelula(estado, m, &linha, &coluna)) { // [CONTROLE] Condição de parada da recursão: não há mais célula vazia.
        return 1; // Não sobrou célula vazia -> resolvido!
    }

    unsigned int candidatos = obterCandidatos(estado, linha, coluna);

    if (candidatos == 0) { // [CONTROLE] Poda antecipada: interrompe esse ramo sem tentar mais nada.
        return 0; // Célula sem nenhum candidato -> esse caminho não funciona.
    }

    for (int valor = 1; valor <= 9; valor++) { // [CONTROLE] For: tenta cada dígito de 1 a 9 que ainda seja candidato.
        if (candidatos & (1 << (valor - 1))) {
            colocarValor(estado, linha, coluna, valor, m); // [EFEITO COLATERAL] Altera tabuleiro e bitmasks (ver função acima).

            if (resolverSudoku(estado, m)) { // [CONTROLE] Recursão: repete o mesmo processo para a próxima célula.
                return 1;
            }

            removerValor(estado, linha, coluna, valor, m); // [EFEITO COLATERAL] Desfaz a tentativa (backtrack) antes de tentar o próximo valor.
        }
    }

    return 0; // Nenhum candidato funcionou -> falha, volta um nível.
}

/*
Verifica se o tabuleiro está completamente preenchido (nenhuma célula com 0).
Usada tanto na validação (case 1, para distinguir "válido" de "válido e
completo") quanto na resolução (case 2, para reconhecer que um tabuleiro
já completo não precisa de busca).
*/
int estaCompleto(int m[9][9]) {
    for (int i = 0; i < 9; i++) {
        for (int j = 0; j < 9; j++) {
            if (m[i][j] == 0) {
                return 0;
            }
        }
    }
    return 1;
}

// Imprime o tabuleiro (usado após a resolução), separando os blocos 3x3 visualmente.
void imprimirTabuleiro(int m[9][9]) { // [EFEITO COLATERAL] Escreve na saída padrão (printf).
    for (int i = 0; i < 9; i++) { // [CONTROLE] For aninhado: percorre as 9x9 células em ordem fixa, imprimindo cada uma.
        for (int j = 0; j < 9; j++) {
            if ((j+1)%3==0) // [CONTROLE] If/else: decide se imprime separador de bloco (a cada 3 colunas) ou espaço normal.
            {
                printf("%d |", m[i][j]);
            }
            else {
            printf("%d ", m[i][j]);
            }
        }
        if ((i+1)%3==0) // [CONTROLE] If/else: decide se imprime a linha separadora de bloco (a cada 3 linhas) ou só a quebra de linha.
        {
            printf("\n---------------------\n");
        }
        else {
        printf("\n");
        }
    }
}


/* ============================================================
FUNÇÕES PRINCIPAIS (leitura de arquivo e validação)
============================================================ */

/*
Lê um tabuleiro 9x9 a partir de um arquivo texto e o grava em tab.
Cada linha do arquivo deve conter exatamente 9 caracteres, sendo '0'
para célula vazia e '1' a '9' para células preenchidas; qualquer
desvio disso (linhas faltando, linhas com tamanho errado ou caracteres
inválidos) faz a função rejeitar o arquivo e retornar 0.
*/
int lerTabuleiro(const char *caminhoArquivo, int tab[9][9]) { // [EFEITO COLATERAL] Abre/lê arquivo externo e escreve em tab (recebido por referência).
    FILE *arquivo = fopen(caminhoArquivo, "r");
    if (arquivo == NULL) {
        printf("Erro: nao foi possivel abrir o arquivo.\n");
        return 0;
    }

    char linha[16]; // Margem de segurança para \n, \r, etc.

    for (int i = 0; i < 9; i++) { // [CONTROLE] For: lê exatamente 9 linhas do arquivo (contagem fixa).
        if (fgets(linha, sizeof(linha), arquivo) == NULL) {
            printf("Erro: arquivo com menos de 9 linhas.\n");
            fclose(arquivo);
            return 0;
        }

        /*
        Garante que a linha tem exatamente 9 caracteres de tabuleiro (nem
        mais, nem menos) antes do fim de linha. Isso impede, por exemplo,
        que um valor de dois dígitos (ex: "10") seja lido como se fossem
        duas células separadas ("1" e "0") - a linha inteira é rejeitada
        em vez de ser silenciosamente mal interpretada.
        */
        size_t tamanhoLinha = strcspn(linha, "\r\n"); // [CONTROLE] Mede o conteúdo da linha, ignorando \r e \n.
        if (tamanhoLinha != 9) {
            printf("Erro: linha %d deve conter exatamente 9 caracteres (encontrado %zu).\n", i + 1, tamanhoLinha);
            fclose(arquivo);
            return 0;
        }

        for (int j = 0; j < 9; j++) { // [CONTROLE] For: converte cada um dos 9 caracteres da linha lida.
            char c = linha[j];
            if (c == '0') {
                tab[i][j] = 0; // [ESTADO] Altera o tabuleiro recebido por parâmetro (célula vazia).
            } else if (c >= '1' && c <= '9') {
                tab[i][j] = c - '0'; // [ESTADO] Altera o tabuleiro recebido por parâmetro (célula preenchida).
            } else {
                printf("Erro: caractere invalido na linha %d.\n", i + 1);
                fclose(arquivo);
                return 0;
            }
        }
    }

    fclose(arquivo); // [EFEITO COLATERAL] Fecha o arquivo (libera o recurso do sistema operacional).
    return 1;
}

/*
Valida um tabuleiro contra as três regras do Sudoku (linha, coluna e
bloco). Para cada dígito de 1 a 9, agrupamos todas as posições onde
ele aparece (lista Qtddig) e comparamos cada par de posições entre si:
se compartilharem linha, coluna ou bloco, há um conflito, registrado
na lista de erros (ListErro).
*/
int ValTabuleiro(int m[9][9], NoE **ListErro) { // [EFEITO COLATERAL] Popula a lista apontada por ListErro (saída via ponteiro-para-ponteiro).
    NoD *Qtddig;
    int SameL, SameC, SameB;
    int totalErros = 0;

    for (int digito = 1; digito <= 9; digito++) { // [CONTROLE] For: repete todo o processo para cada dígito de 1 a 9.
        Qtddig = NULL;

        // Monta a lista de todas as posições onde esse dígito aparece.
        for (int j = 0; j < 9; j++) { // [CONTROLE] For aninhado: varre as 81 células procurando o dígito atual.
            for (int k = 0; k < 9; k++) {
                if (m[j][k] == digito) {
                    InserirFimDigTab(&Qtddig, digito, j, k); // [EFEITO COLATERAL] Altera a lista local Qtddig.
                }
            }
        }

        /*
        Compara cada posição encontrada com todas as posteriores
        (começar "aux" em "a->prox" evita comparar uma célula com ela
        mesma e evita reportar o mesmo par de erro duas vezes).
        */
        NoD *a = Qtddig;
        while (a != NULL) { // [CONTROLE] While: percorre a lista de posições do dígito atual.
            NoD *aux = a->prox;
            while (aux != NULL) { // [CONTROLE] While aninhado: compara "a" com cada posição posterior a ele.
                SameL = (a->linha == aux->linha);
                SameC = (a->coluna == aux->coluna);
                SameB = (a->linha / 3 == aux->linha / 3) && (a->coluna / 3 == aux->coluna / 3);

                if (SameL || SameC || SameB) {
                    InserirFimErrosTab(ListErro, digito, a->linha, a->coluna, aux->linha, aux->coluna); // [EFEITO COLATERAL] Altera *ListErro (o chamador enxerga esse erro).
                    totalErros++; // [ESTADO] Atualiza o contador local de erros.
                }

                aux = aux->prox;
            }
            a = a->prox;
        }

        FreelistDigTab(&Qtddig); // [EFEITO COLATERAL] Libera a lista temporária antes do próximo dígito.
    }

    return totalErros; // 0 = tabuleiro válido, >0 = quantidade de conflitos encontrados.
}


/* ============================================================
FUNÇÃO PRINCIPAL (main)
============================================================ */

/*
Ponto de entrada do programa. Lê o tabuleiro de "tabuleiro.txt", pergunta
ao usuário qual ação deseja executar (validar, resolver ou validar
resposta) e direciona a execução para o bloco correspondente do switch.
*/
int main() {
    int dec;
    // Lê o tabuleiro no arquivo .txt e o insere na matriz 9x9.
    if (!lerTabuleiro("tabuleiro.txt", tabuleiro)) { // [EFEITO COLATERAL] Chamada altera a variável global "tabuleiro".
        return 1;
    }

    /*
    O usuário digita uma das três ações possíveis:
    1 - validar tabuleiro (verifica se o tabuleiro ainda não preenchido cumpre com as regras do jogo Sudoku);
    2 - resolver tabuleiro (resolve o tabuleiro Sudoku fornecido no arquivo "tabuleiro.txt");
    3 - validar resposta (verifica se o tabuleiro em "tabuleiro.txt" e "tabuleiro2.txt" possuem os mesmos valores fixos e depois verifica se a resposta é válida).
    */
    printf("Digite a seguir qual acao queira fazer:\n1-Validar tabuleiro\n2-Resolver tabuleiro\n3-Validar resposta(insira o tabuleiro resolvido no arquivo \"tabuleiro2.txt\")\nDigite aqui:");
    scanf("%d", &dec); // [EFEITO COLATERAL] Lê entrada do usuário (I/O) e altera "dec".

    // Loop que irá funcionar enquanto a variável "dec" não recebe algum valor aceitável das três possíveis ações.
    while (dec != 1 && dec != 2 && dec != 3) { // [CONTROLE] While: repete a leitura até receber uma opção válida (1, 2 ou 3).
        printf("Acao nao encontrada, tente novamente:");
        scanf("%d", &dec); // [EFEITO COLATERAL] Lê nova entrada do usuário.
    }

    switch (dec) { // [CONTROLE] Switch: direciona a execução para uma das três ações, conforme a escolha do usuário.
        case 1: // Validação de tabuleiro.
        {
            NoE *listaErros = NULL;
            int erros = ValTabuleiro(tabuleiro, &listaErros); // [EFEITO COLATERAL] Popula listaErros.

            if (erros == 0) { // [CONTROLE] If/else: decide qual mensagem imprimir conforme o resultado da validação.
                if (estaCompleto(tabuleiro)) { // [CONTROLE] Diferencia "válido" de "válido e completo" (CST-02).
                    printf("Tabuleiro valido e completo!\n");
                } else {
                    printf("Tabuleiro valido!\n");
                }
            } else {
                PrintErrosTab(listaErros); // [EFEITO COLATERAL] Imprime a lista de erros.
            }

            FreelistErroTab(&listaErros); // [EFEITO COLATERAL] Libera a memória da lista de erros.
            break;
        }

        case 2: // Resolução de tabuleiro.
        {
            /*
            Antes de tentar resolver, garante que o tabuleiro inicial é válido.
            Rodar o backtracking sobre um tabuleiro já inconsistente não faz sentido.
            */
            NoE *listaErros = NULL;
            int erros = ValTabuleiro(tabuleiro, &listaErros);

            if (erros > 0) { // [CONTROLE] If: interrompe o case cedo se o tabuleiro inicial já for inválido.
                printf("Tabuleiro invalido, nao e possivel resolver:\n");
                PrintErrosTab(listaErros);
                FreelistErroTab(&listaErros);
                break;
            }
            FreelistErroTab(&listaErros);

            /*
            Se o tabuleiro já chega completo (e já sabemos que é válido, pela
            checagem acima), não há nada a resolver - evita chamar o backtracking
            à toa e permite uma mensagem específica para esse caso (CST-11).
            */
            if (estaCompleto(tabuleiro)) {
                printf("O tabuleiro ja estava completo e valido. Nenhuma busca foi necessaria.\n");
                imprimirTabuleiro(tabuleiro);
                break;
            }

            EstadoBitmask estado; // [ESTADO] Cria o estado auxiliar local, usado só durante esta resolução.
            inicializarBitmasks(&estado, tabuleiro); // [EFEITO COLATERAL] Preenche "estado" a partir do tabuleiro atual.

            if (resolverSudoku(&estado, tabuleiro)) { // [EFEITO COLATERAL] Modifica "tabuleiro" e "estado" durante toda a recursão.
                printf("Solucao encontrada:\n");
                imprimirTabuleiro(tabuleiro);
            } else {
                printf("Este tabuleiro nao possui solucao.\n");
            }

            break;
        }

        case 3: // Validação de resposta.
        {
            int tabuleiroResposta[9][9]; // [ESTADO] Estado local, criado apenas para este case.

            if (!lerTabuleiro("tabuleiro2.txt", tabuleiroResposta)) { // [EFEITO COLATERAL] Preenche tabuleiroResposta a partir do arquivo.
                break;
            }

            // Verifica se o tabuleiro está completo (nenhuma célula vazia).
            int completo = 1;
            for (int i = 0; i < 9 && completo; i++) { // [CONTROLE] For com condição extra: para cedo assim que acha uma célula vazia.
                for (int j = 0; j < 9; j++) {
                    if (tabuleiroResposta[i][j] == 0) {
                        completo = 0; // [ESTADO] Atualiza a flag local.
                        break;
                    }
                }
            }

            /*
            Verifica se a resposta respeita as pistas do tabuleiro original
            (uma solução "sem conflitos" não é válida se alterou uma pista fixa).
            */
            int respeitaPistas = 1;
            for (int i = 0; i < 9 && respeitaPistas; i++) { // [CONTROLE] For com condição extra: para cedo assim que acha uma divergência.
                for (int j = 0; j < 9; j++) {
                    if (tabuleiro[i][j] != 0 && tabuleiro[i][j] != tabuleiroResposta[i][j]) {
                        respeitaPistas = 0; // [ESTADO] Atualiza a flag local.
                        break;
                    }
                }
            }

            // Verifica infrações de regras (linha, coluna, bloco).
            NoE *listaErros = NULL;
            int erros = ValTabuleiro(tabuleiroResposta, &listaErros); // [EFEITO COLATERAL] Popula listaErros.

            if (!completo) { // [CONTROLE] If/else if/else: decide qual mensagem final imprimir, em ordem de prioridade.
                printf("Resposta invalida: existem celulas vazias.\n");
            } else if (!respeitaPistas) {
                printf("Resposta invalida: nao respeita os valores originais do tabuleiro.\n");
            } else if (erros > 0) {
                printf("Resposta invalida: ha conflitos.\n");
                PrintErrosTab(listaErros);
            } else {
                printf("Resposta valida! Parabens.\n");
            }

            FreelistErroTab(&listaErros); // [EFEITO COLATERAL] Libera a memória da lista de erros.
            break;
        }
    }

    return 0; // Fim da execução do programa.
}