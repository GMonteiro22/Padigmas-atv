[P4-ETAPA-06]

# Reflexão: de "que estado devo modificar?" para "que transformação deve ser aplicada aos dados?"

## Como a solução era pensada antes

Nas versões imperativa (C) e orientada a objetos (Java), o desenvolvimento partia sempre da mesma pergunta: o que precisa ser alterado em cada passo do programa.

Em C, o tabuleiro era uma matriz global. Para resolver o Sudoku, o programa preenchia células dessa matriz, atualizava três vetores de bitmask ao mesmo tempo e, quando uma tentativa falhava, desfazia as alterações com a função `removerValor`, respeitando a ordem correta. Em Java, a mutação ficou concentrada dentro dos objetos, mas continuou existindo. Os métodos `cobrir()` e `descobrir()` alteravam ponteiros da malha de Dancing Links e precisavam ser exatamente o inverso um do outro.

Nas duas versões, uma parte considerável do trabalho estava em manter o estado consistente. Era preciso lembrar de desfazer cada alteração, liberar a memória alocada e evitar que uma função modificasse um dado que outra ainda iria utilizar. Essas preocupações não fazem parte do problema do Sudoku em si, mas eram necessárias para que o programa funcionasse corretamente.

## Como a solução passou a ser pensada

Em Haskell, os dados não podem ser modificados. Por isso, a pergunta inicial deixa de ser o que alterar e passa a ser qual resultado deve ser obtido a partir de um dado existente. Essa mudança afetou diretamente cada parte do código.

### Atualização de uma célula

Em C, preencher uma célula era feito com uma atribuição, como `m[linha][coluna] = valor`, que altera o tabuleiro existente. Em Haskell, a função `definirValor` recebe um tabuleiro e devolve um novo tabuleiro, igual ao anterior exceto pela célula indicada. O tabuleiro original continua disponível e sem alterações.

### Backtracking sem etapa de desfazer

Esta foi a diferença mais significativa entre as versões. Em C, cada tentativa usava o par `colocarValor` e `removerValor`, pois todas as tentativas compartilhavam o mesmo tabuleiro. Em Haskell, a tentativa é escrita da seguinte forma:

```haskell
tentar p d = buscar (definirValor p d tab) (marcar m (p, d))
```

Cada tentativa recebe sua própria versão do tabuleiro e das máscaras. Quando uma tentativa falha, ela apenas retorna `Nothing` e é descartada. Como o tabuleiro do nível anterior nunca foi modificado, não existe nada a ser desfeito. O retrocesso, que em C era uma operação explícita e sujeita a erros, passa a acontecer pela própria estrutura da recursão.

A busca também passou a ser descrita como uma transformação sobre uma lista. A expressão `listToMaybe (mapMaybe tentar candidatos)` transforma a lista de candidatos em uma lista de soluções possíveis e seleciona a primeira. Como Haskell utiliza avaliação preguiçosa, os candidatos seguintes não chegam a ser avaliados depois que uma solução é encontrada. O efeito é equivalente ao `return 1` dentro do laço da versão em C.

### Construção das máscaras

Em C, as máscaras eram montadas por um laço duplo que aplicava `|=` sobre uma struct recebida por ponteiro. Em Haskell, essa construção é feita com `foldl' marcar mascaraVazia pistas`. Em vez de definir uma variável para acumular o resultado, a solução define como cada pista transforma um conjunto de máscaras em outro. O `foldl'` aplica essas transformações em sequência.

### Validação

Em C, a validação montava uma lista encadeada de posições com `malloc`, percorria essa lista com dois ponteiros, inseria os erros encontrados em outra lista e, ao final, liberava toda a memória utilizada. Em Haskell, a validação é uma sequência de transformações sobre listas: `filter` seleciona as posições onde um dígito aparece, `paresDe` gera os pares dessas posições, uma condição mantém apenas os pares em conflito e `concatMap` reúne os resultados dos nove dígitos. Nenhuma estrutura precisa ser criada e depois destruída, já que cada etapa apenas recebe dados e produz novos dados.

## Separação entre o núcleo e a interação externa

Pensar a solução em termos de transformações também exigiu separar com clareza o que é cálculo e o que é interação com o ambiente externo. Em C, a função `lerTabuleiro` lia o arquivo, validava o formato e imprimia mensagens de erro, tudo na mesma função. Da mesma forma, `ValTabuleiro` identificava os erros enquanto alterava listas por meio de ponteiros.

Na versão em Haskell, essas responsabilidades foram separadas. A função `interpretarTabuleiro` recebe o texto do arquivo e devolve um `Either String Tabuleiro`, sem qualquer relação com a leitura do arquivo propriamente dita. As funções `relatorioValidacao`, `relatorioResolucao` e `relatorioResposta` produzem textos, mas não os imprimem. Somente `lerTabuleiro`, `lerOpcao`, `executar` e `main` interagem com o ambiente externo, e se limitam a ler dados ou exibir textos já preparados pelo núcleo puro.

Com essa organização, as funções do núcleo podem ser testadas de forma isolada. Qualquer uma delas pode ser chamada no `ghci` com um tabuleiro informado diretamente, sem a necessidade de arquivos ou de entrada pelo teclado.

## O que permaneceu igual

A estratégia de resolução continuou a mesma utilizada em C: bitmasks para consultar os candidatos de cada célula, a heurística MRV para escolher a célula mais restrita e backtracking para explorar as possibilidades. As operações de bits do Haskell (`.|.`, `.&.`, `complement`, `testBit` e `popCount`) correspondem diretamente aos operadores `|`, `&`, `~` e à contagem de bits da versão em C. Isso indica que a mudança de paradigma não alterou o algoritmo. O que mudou foi a forma de expressá-lo, agora sem variáveis que têm seu valor modificado.

## Limitações da abordagem

A abordagem funcional também apresentou algumas desvantagens. A primeira está relacionada às cópias: cada tentativa cria um novo tabuleiro e um novo conjunto de máscaras. Como foram utilizadas listas, a função `definirValor` e o acesso com `!!` têm custo linear, enquanto em C esses acessos eram feitos diretamente na matriz. Para um tabuleiro 9x9 essa diferença é pequena, mas em tabuleiros maiores seria recomendável o uso de estruturas mais adequadas, como `Data.Array` ou `Data.Map`.

A segunda está na forma de raciocinar. Representar o backtracking como uma lista de tentativas avaliada de forma preguiçosa exige um nível de abstração maior do que um laço com retorno antecipado, especialmente para quem está habituado ao paradigma imperativo.

## Conclusão

Ao substituir a pergunta "que estado devo modificar?" por "que transformação deve ser aplicada aos dados?", a resolução do problema deixou de ser uma sequência de alterações a coordenar e passou a ser uma composição de funções que levam um tabuleiro a outro. O principal ganho foi a eliminação da lógica de desfazer alterações e liberar memória. Em C, essa lógica era indispensável para a correção do programa. Em Haskell, ela não é necessária, uma vez que nenhum dado é modificado.
