[P4-ETAPA-04]

# Reflexão: como meu modelo mudou ao passar do paradigma imperativo para o orientado a objetos?

## Visão geral

Na versão imperativa (C), o programa era uma coleção de funções que manipulavam dados compartilhados: um tabuleiro global, uma struct de bitmasks passada por ponteiro e listas encadeadas montadas à mão. A resolução era um backtracking com heurística MRV e bitmasks, e a função recursiva conhecia, ao mesmo tempo, o jeito de buscar e as regras do Sudoku.

Na versão orientada a objetos (Java) não fiz uma tradução dessas funções para métodos. Refiz a modelagem em torno de uma ideia diferente: traduzir o Sudoku para um problema genérico de cobertura exata e resolvê-lo com o Algorithm X de Knuth (Dancing Links). Com isso, as responsabilidades foram redistribuídas entre objetos que colaboram entre si, e o próprio código está organizado em três pacotes que espelham essa divisão:

| Pacote | Classes | Papel |
|---|---|---|
| `Objeto` | `Tabuleiro` | Representa o estado do tabuleiro e as perguntas que ele sabe responder sobre si mesmo |
| `validar` | `ErroValidacao`, `RegraValidacao`, `RegraLinha`, `RegraColuna`, `RegraBloco`, `Validador` | Tudo relacionado a checar se um tabuleiro respeita as regras do Sudoku |
| `resolver` | `No`, `ColunaCabecalho`, `MatrizCoberturaExata`, `ConstrutorCoberturaSudoku`, `SudokuSolver` | Tudo relacionado a encontrar uma solução |
| `Principal` | `Aplicacao` | Menu e fluxo principal, orquestrando os três pacotes acima |

Essa separação em pacotes não é só organização de arquivo: ela força uma fronteira física entre "o que é o tabuleiro", "o que é validar" e "o que é resolver" - as classes de `resolver` não enxergam nada de `validar`, e vice-versa, só se encontram através de `Aplicacao`.

## Representação do estado

**Em C**, o estado central era `int tabuleiro[9][9]`, uma variável global. Qualquer função podia lê-la ou alterá-la, e o backtracking a modificava diretamente a cada tentativa (`colocarValor` e `removerValor`). Havia um segundo estado auxiliar, a struct `EstadoBitmask`, que espelhava a mesma informação de forma compacta e precisava ser mantida em sincronia com o tabuleiro. Os erros de validação viviam em listas encadeadas alocadas manualmente com `malloc` e liberadas com `free`.

**Em Java**, o estado ficou distribuído em objetos, cada um dono do seu pedaço:

- A grade vive dentro de `Tabuleiro`, como atributo privado.
- Os conflitos viram objetos `ErroValidacao` imutáveis, guardados numa `List` da biblioteca padrão, sem gerenciamento manual de memória.
- Durante a resolução, o estado deixa de ser um tabuleiro com bitmasks paralelos e passa a ser a própria malha de nós encadeados dentro de `MatrizCoberturaExata`. Não existe mais uma estrutura "espelho" para manter sincronizada.
- A resolução não altera o tabuleiro de entrada: `SudokuSolver.resolver` devolve um novo `Tabuleiro`.

Um ponto de honestidade: mutação não desapareceu. `Tabuleiro` continua tendo `setValor`, e as operações `cobrir()` e `descobrir()` do Dancing Links são, por natureza, alterações de ponteiros. O que mudou foi onde a mutação acontece: ela ficou confinada dentro de objetos que a controlam, em vez de estar espalhada por funções que recebem a matriz por parâmetro.

## Responsabilidades

Em C, uma mesma função acumulava várias responsabilidades. `ValTabuleiro` agrupava as posições de cada dígito, comparava os pares, decidia se havia conflito de linha, coluna ou bloco e registrava o erro. `resolverSudoku` misturava a estratégia de busca (escolher a célula, tentar, recuar) com o conhecimento das regras do Sudoku (consultar os bitmasks de linha, coluna e bloco). O `main` também acumulava lógica de domínio: comparar célula a célula se uma resposta respeitava as pistas originais era um laço duplo escrito diretamente ali.

Em Java, cada classe tem uma responsabilidade principal, e o `main` (aqui, `Aplicacao`) foi enxugado até sobrar só orquestração:

- **Estado do tabuleiro:** `Tabuleiro` não só guarda a grade, como também responde perguntas sobre si mesma - `estaCompleto()` e `respeitaPistasDe(outroTabuleiro)`. Esta última substituiu um laço que antes vivia solto dentro do `case 3` do `main`; a comparação célula a célula entre o tabuleiro original e a resposta do usuário é uma regra do próprio domínio do tabuleiro, não uma tarefa do menu.
- **Validação:** cada regra (`RegraLinha`, `RegraColuna`, `RegraBloco`) sabe verificar uma única dimensão. O `Validador` só coordena.
- **Resolução:** a `MatrizCoberturaExata` sabe apenas resolver cobertura exata, o que faz sem saber o que é linha, coluna ou bloco. O `ConstrutorCoberturaSudoku` concentra todo o conhecimento específico do Sudoku (as 324 restrições e a decodificação das jogadas). O `SudokuSolver` só amarra as duas.
- **Interface com o usuário:** `Aplicacao` cuida do menu e das mensagens, delegando toda a lógica de negócio para `Tabuleiro`, `Validador` e `SudokuSolver`.

Essa separação entre "como resolver", "como validar" e "o que é o problema" é a mudança mais profunda entre as duas versões, e não tem equivalente no código em C.

## Relacionamento entre componentes

Em C, os componentes se relacionavam por chamadas de função e por dados compartilhados (a global e os ponteiros). Em Java, as relações têm tipo, e a divisão em pacotes deixa essas relações visíveis mesmo antes de abrir qualquer arquivo:

- **Composição:** `MatrizCoberturaExata` cria e é dona da sua malha de colunas e nós; eles não fazem sentido fora dela.
- **Agregação:** `Validador` mantém uma lista de `RegraValidacao`; as regras existem como objetos independentes e poderiam ser usadas isoladamente.
- **Herança:** `ColunaCabecalho` herda de `No` (justificativa na seção seguinte).
- **Realização de interface:** as três regras implementam `RegraValidacao`.
- **Dependência entre pacotes:** `validar` e `resolver` dependem de `Objeto` (para receber um `Tabuleiro`), mas não dependem um do outro - só `Aplicacao` conhece os dois ao mesmo tempo.

## Herança: onde usei e onde deliberadamente não usei

A atividade pede que herança não seja usada só para cumprir requisito. Usei em um único lugar, onde ela descreve a estrutura real do problema: `ColunaCabecalho extends No`.

**Por que essa herança é genuína:** um cabeçalho de coluna participa da malha exatamente como um nó comum. Ele usa os campos `esquerda` e `direita` para se encadear na fileira horizontal de cabeçalhos, e `cima` e `baixo` como topo da sua coluna vertical. A relação "é um" vale de fato, e é a modelagem canônica de Knuth. A subclasse acrescenta o que um nó comum não tem (`tamanho`, `nome`, `cobrir()`, `descobrir()`).

**Onde escolhi não usar herança:** as três regras de validação poderiam ter uma superclasse abstrata comum, já que a estrutura de cada uma (varrer, coletar posições de um dígito, comparar pares) é parecida. Optei por uma interface e três classes independentes. Herdar ali serviria só para reaproveitar um trecho de código, e isso acoplaria as regras entre si. Aceitei uma pequena repetição em troca de regras totalmente independentes. Pelo mesmo motivo, `Validador` compõe as regras em vez de herdar de algo.

**Sobre polimorfismo:** o polimorfismo efetivamente exercitado está na interface. `Validador` chama `regra.verificar(...)` sobre referências do tipo `RegraValidacao` sem saber qual implementação concreta está rodando. A herança `ColunaCabecalho`/`No` não sobrescreve métodos, então ela demonstra reaproveitamento de estrutura, não polimorfismo de comportamento.

## Reutilização

- **`MatrizCoberturaExata` é genérica.** Ela não menciona Sudoku em nenhum lugar. Para resolver outro problema de cobertura exata (posicionamento de rainhas, ladrilhamento), bastaria escrever um novo construtor que traduza aquele problema para colunas e linhas, sem tocar no algoritmo. Em C isso não seria possível sem reescrever a busca, porque as regras do Sudoku estavam embutidas nela.
- **`Tabuleiro` serve a três papéis:** o tabuleiro original, a resposta do usuário e a solução gerada são todos instâncias da mesma classe, e `respeitaPistasDe` funciona entre quaisquer dois deles.
- **Um único `Validador` atende as três ações do menu**, e cada regra pode ser usada sozinha.
- Em C também havia reaproveitamento (por exemplo, `lerTabuleiro` recebendo o destino por parâmetro), mas no nível de função. Em Java ele acontece no nível de componente, e a separação em pacotes reforça que `resolver` e `validar` poderiam, em tese, ser extraídos como bibliotecas independentes.

## Encapsulamento

**Onde o encapsulamento ficou forte:**

- `Tabuleiro` esconde a grade (`private final`). Quem precisa de um valor usa `getValor`, e nenhum outro código conhece a representação interna. Se ela mudasse para um vetor de 81 posições, só `Tabuleiro` seria alterada.
- A comparação de pistas entre dois tabuleiros (`respeitaPistasDe`) também deixou de ser uma responsabilidade externa: antes, `Aplicacao` precisava conhecer que "comparar duas grades célula a célula" era a forma de checar isso; agora ela só pergunta ao `Tabuleiro`, sem saber como a resposta é calculada por dentro.
- `ErroValidacao` é imutável: campos `private final` e nenhum setter.
- Em `Validador` e `MatrizCoberturaExata`, os atributos internos (`regras`, `cabecalhoRaiz`, `colunas`) são privados.
- O estado dos bitmasks, que em C circulava por ponteiro entre funções, desapareceu da interface pública.

**Onde ficou fraco, e por quê:** os campos de `No` (`esquerda`, `direita`, `cima`, `baixo`, `coluna`, `idLinha`) e de `ColunaCabecalho` (`tamanho`, `nome`) são públicos. Mantive assim porque o Dancing Links depende de manipular esses ponteiros o tempo todo e de forma muito direta, e o código fica mais próximo do algoritmo original de Knuth. Isso é uma concessão consciente de clareza sobre encapsulamento estrito. Uma melhoria natural seria tornar esses campos privados dentro do próprio pacote `resolver`, expondo apenas as operações (`cobrir`, `descobrir`, `inserirAbaixoDe`).

## Extensão do sistema

**Nova regra de validação.** Para suportar, por exemplo, um Sudoku com restrição de diagonais, basta criar uma classe no pacote `validar` que implemente `RegraValidacao` e cadastrá-la com `validador.adicionarRegra(...)`. Nenhuma classe existente precisa ser alterada, o que é o princípio aberto-fechado na prática. Em C, o mesmo ajuste exigiria editar `ValTabuleiro`.

**Novo problema de cobertura exata.** Como visto na reutilização, bastaria um novo construtor no pacote `resolver`, sem tocar em `MatrizCoberturaExata`.

**Limites da extensibilidade atual (o que ainda não está bom):**

- **As regras de Sudoku estão duplicadas em dois lugares.** As três `RegraValidacao` conhecem as regras para validar, e o `ConstrutorCoberturaSudoku` as conhece de novo, de outra forma, para gerar as 324 colunas. Adicionar a regra das diagonais exigiria mexer nos dois. Uma evolução possível seria cada regra também saber gerar as colunas de cobertura exata correspondentes.
- **`SudokuSolver` é uma classe concreta, não uma interface.** Trocar a estratégia de resolução (por exemplo, voltar ao backtracking com bitmasks) exigiria extrair uma interface primeiro. Não fiz isso.
- **O tamanho 9x9 está fixo em vários pontos do código.** Suportar outros tamanhos de tabuleiro exigiria parametrizar `Tabuleiro`, as regras e o construtor.

## Conclusão

O modelo mudou de "dados globais manipulados por funções" para "objetos com responsabilidades separadas que colaboram por contratos, organizados em pacotes que tornam essas fronteiras visíveis". Os ganhos mais claros foram a separação entre o algoritmo genérico e o domínio (`MatrizCoberturaExata` versus `ConstrutorCoberturaSudoku`), a validação extensível por interface, o desaparecimento do gerenciamento manual de listas, e a migração de regras de negócio que estavam soltas no `main` (como a comparação de pistas) para dentro do objeto a que pertencem. Em contrapartida, aceitei campos públicos nos nós do Dancing Links e deixei duplicação de conhecimento entre validação e construção da matriz. Ambos são pontos que eu melhoraria numa próxima iteração.
