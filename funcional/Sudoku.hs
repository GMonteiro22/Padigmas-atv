{-
Resolvedor de Sudoku - paradigma funcional
Estratégia utilizada: bitmask + MRV + backtracking

Legenda:
[PURA] função pura | [IMPURA] função com efeitos (IO) | [IMUTÁVEL] dado imutável
[TRANSFORMAÇÃO] transforma dados | [ALTA ORDEM] usa ou recebe funções
[PRIMEIRA CLASSE] função tratada como valor (passada, guardada ou criada na hora)
[COMPOSIÇÃO] composição funcional | [MUNDO EXTERNO] interação com arquivo/teclado/tela
-}
module Main where

import Control.Exception (IOException, try)
import Data.Bits (bit, complement, popCount, testBit, (.&.), (.|.))
import Data.Char (digitToInt, isDigit)
import Data.List (foldl', minimumBy)
import Data.Maybe (listToMaybe, mapMaybe)
import Data.Ord (comparing)
import System.IO (hFlush, stdout)
import Text.Read (readMaybe)


-- ============================================================
-- TIPOS E DADOS
-- ============================================================

-- [IMUTÁVEL] O tabuleiro é uma lista de 9 linhas com 9 inteiros (0 = vazia).
-- Nenhuma função altera um tabuleiro: "alterar" significa produzir um novo.
type Tabuleiro = [[Int]]

-- [IMUTÁVEL] Posição (linha, coluna), com índices de 0 a 8.
type Posicao = (Int, Int)

-- [IMUTÁVEL] Um conflito: o dígito repetido e as duas posições envolvidas.
data Erro = Erro Int Posicao Posicao

-- [IMUTÁVEL] Bitmasks de dígitos já usados por linha, coluna e bloco.
-- Equivale à struct EstadoBitmask do C, mas cada atualização gera uma cópia nova.
data Mascaras = Mascaras [Int] [Int] [Int]


-- ============================================================
-- FUNÇÕES AUXILIARES (todas puras)
-- ============================================================

-- [IMUTÁVEL] Índices válidos de linha/coluna e lista das 81 posições.
indices :: [Int]
indices = [0 .. 8]

posicoes :: [Posicao]
posicoes = [(l, c) | l <- indices, c <- indices]

-- [PURA] Lê o valor de uma posição do tabuleiro.
valorEm :: Tabuleiro -> Posicao -> Int
valorEm tab (l, c) = tab !! l !! c

-- [PURA] Calcula o índice do bloco 3x3 (0 a 8), mesma fórmula do C e do Java.
indiceBloco :: Posicao -> Int
indiceBloco (l, c) = (l `div` 3) * 3 + c `div` 3

-- [PURA] [ALTA ORDEM] [PRIMEIRA CLASSE] Devolve uma NOVA lista com o elemento i transformado por f.
-- Recebe a transformação f como valor; substitui a atribuição "x[i] = ..." do imperativo.
atualizarEm :: Int -> (a -> a) -> [a] -> [a]
atualizarEm i f xs = [if j == i then f x else x | (j, x) <- zip [0 ..] xs]

-- [PURA] [TRANSFORMAÇÃO] Gera um novo tabuleiro com o valor v na posição dada.
-- O tabuleiro original permanece intacto (não há "removerValor" no backtracking).
definirValor :: Posicao -> Int -> Tabuleiro -> Tabuleiro
definirValor (l, c) v = atualizarEm l (atualizarEm c (const v))

-- [PURA] [ALTA ORDEM] Tabuleiro completo = nenhuma linha contém o valor 0.
-- "notElem 0" é aplicação parcial: vira uma função passada para "all".
estaCompleto :: Tabuleiro -> Bool
estaCompleto = all (notElem 0)

-- [PURA] [ALTA ORDEM] Verifica se a resposta mantém todas as pistas do original.
-- Achata as duas grades e compara célula a célula com zipWith.
respeitaPistas :: Tabuleiro -> Tabuleiro -> Bool
respeitaPistas original resposta = and (zipWith compativel (concat original) (concat resposta))
  where
    compativel o r = o == 0 || o == r


-- ============================================================
-- VALIDAÇÃO (núcleo puro)
-- ============================================================

-- [PURA] Duas posições conflitam se compartilham linha, coluna ou bloco.
mesmaUnidade :: Posicao -> Posicao -> Bool
mesmaUnidade a@(l1, c1) b@(l2, c2) = l1 == l2 || c1 == c2 || indiceBloco a == indiceBloco b

-- [PURA] [TRANSFORMAÇÃO] Gera todos os pares (a, b) sem repetição e sem comparar um item consigo.
-- Mesma ideia de "aux = a->prox" do C, agora expressa como lista por compreensão.
paresDe :: [a] -> [(a, a)]
paresDe xs = [(a, b) | (i, a) <- zip [0 ..] xs, b <- drop (i + 1) xs]

-- [PURA] [ALTA ORDEM] [COMPOSIÇÃO] Lista os conflitos de um único dígito.
-- Filtra as posições onde o dígito aparece e mantém os pares que conflitam.
errosDoDigito :: Tabuleiro -> Int -> [Erro]
errosDoDigito tab d = [Erro d a b | (a, b) <- paresDe ondeAparece, mesmaUnidade a b]
  where
    ondeAparece = filter ((== d) . valorEm tab) posicoes

-- [PURA] [ALTA ORDEM] Validação completa: junta os conflitos dos dígitos 1 a 9.
-- Substitui ValTabuleiro e as listas encadeadas manuais (NoD/NoE) do C.
validar :: Tabuleiro -> [Erro]
validar tab = concatMap (errosDoDigito tab) [1 .. 9]


-- ============================================================
-- RESOLUÇÃO: bitmask + MRV + backtracking (núcleo puro)
-- ============================================================

-- [IMUTÁVEL] Máscaras iniciais, sem nenhum dígito marcado.
mascaraVazia :: Mascaras
mascaraVazia = Mascaras (replicate 9 0) (replicate 9 0) (replicate 9 0)

-- [PURA] [TRANSFORMAÇÃO] [PRIMEIRA CLASSE] Devolve NOVAS máscaras com o dígito d marcado.
-- A seção (.|. bit (d - 1)) é uma função criada na hora e passada para atualizarEm.
marcar :: Mascaras -> (Posicao, Int) -> Mascaras
marcar (Mascaras ls cs bs) (p@(l, c), d) = Mascaras (liga l ls) (liga c cs) (liga (indiceBloco p) bs)
  where
    liga i = atualizarEm i (.|. bit (d - 1))

-- [PURA] [ALTA ORDEM] Monta as máscaras a partir das pistas usando um fold.
-- O fold substitui o laço que fazia "|=" repetidamente sobre a struct no C.
mascarasIniciais :: Tabuleiro -> Mascaras
mascarasIniciais tab = foldl' marcar mascaraVazia [(p, v) | p <- posicoes, let v = valorEm tab p, v /= 0]

-- [PURA] Bitmask dos dígitos ainda livres numa posição (OR, NOT e AND com 0x1FF).
-- Mesma operação de obterCandidatos no C.
candidatos :: Mascaras -> Posicao -> Int
candidatos (Mascaras ls cs bs) p@(l, c) = complement ocupados .&. 0x1FF
  where
    ocupados = (ls !! l) .|. (cs !! c) .|. (bs !! indiceBloco p)

-- [PURA] [ALTA ORDEM] [COMPOSIÇÃO] Converte um bitmask na lista de dígitos que ele contém.
digitosDe :: Int -> [Int]
digitosDe mascara = filter (testBit mascara . subtract 1) [1 .. 9]

-- [PURA] [ALTA ORDEM] Heurística MRV: escolhe a célula vazia com menos candidatos.
-- Devolve Nothing quando não há célula vazia (tabuleiro resolvido).
escolherCelula :: Tabuleiro -> Mascaras -> Maybe (Posicao, Int)
escolherCelula tab m = case vazias of
  [] -> Nothing
  _ -> Just (minimumBy (comparing (popCount . snd)) vazias)
  where
    vazias = [(p, candidatos m p) | p <- posicoes, valorEm tab p == 0]

-- [PURA] [ALTA ORDEM] [PRIMEIRA CLASSE] Backtracking recursivo: cada tentativa recebe um tabuleiro novo.
-- "tentar p" é passada como valor ao mapMaybe; um ramo que falha é só descartado (sem "desfazer").
buscar :: Tabuleiro -> Mascaras -> Maybe Tabuleiro
buscar tab m = case escolherCelula tab m of
  Nothing -> Just tab
  Just (p, cands) -> listToMaybe (mapMaybe (tentar p) (digitosDe cands))
  where
    tentar p d = buscar (definirValor p d tab) (marcar m (p, d))

-- [PURA] Ponto de entrada da resolução: Just solução, ou Nothing se não houver.
resolver :: Tabuleiro -> Maybe Tabuleiro
resolver tab = buscar tab (mascarasIniciais tab)


-- ============================================================
-- FORMATAÇÃO E RELATÓRIOS (puros: produzem texto, não imprimem)
-- ============================================================

-- [PURA] [TRANSFORMAÇÃO] Converte o tabuleiro em texto com separadores de bloco.
mostrarTabuleiro :: Tabuleiro -> String
mostrarTabuleiro tab = concat (zipWith formatarLinha [1 :: Int ..] tab)
  where
    formatarLinha i linha =
      concat (zipWith formatarCelula [1 :: Int ..] linha)
        ++ "\n"
        ++ (if i `mod` 3 == 0 then "---------------------\n" else "")
    formatarCelula j v = show v ++ (if j `mod` 3 == 0 then " |" else " ")

-- [PURA] Formata um conflito no padrão das outras versões (índices de 1 a 9).
mostrarErro :: Erro -> String
mostrarErro (Erro d (l1, c1) (l2, c2)) =
  "O digito " ++ show d ++ " nos pontos [" ++ show (l1 + 1) ++ "," ++ show (c1 + 1)
    ++ "] - [" ++ show (l2 + 1) ++ "," ++ show (c2 + 1) ++ "]"

-- [PURA] [ALTA ORDEM] Monta o texto com a lista completa de conflitos.
listarErros :: [Erro] -> String
listarErros erros = unlines ("Os erros encontrados no tabuleiro foram:" : map mostrarErro erros)

-- [PURA] Ação 1: texto do resultado da validação.
relatorioValidacao :: Tabuleiro -> String
relatorioValidacao tab = case validar tab of
  []
    | estaCompleto tab -> "Tabuleiro valido e completo!\n"
    | otherwise -> "Tabuleiro valido!\n"
  erros -> listarErros erros

-- [PURA] [COMPOSIÇÃO] Ação 2: valida, checa completude e, se preciso, resolve.
relatorioResolucao :: Tabuleiro -> String
relatorioResolucao tab
  | not (null erros) = "Tabuleiro invalido, nao e possivel resolver:\n" ++ listarErros erros
  | estaCompleto tab =
      "O tabuleiro ja estava completo e valido. Nenhuma busca foi necessaria.\n" ++ mostrarTabuleiro tab
  | otherwise =
      maybe "Este tabuleiro nao possui solucao.\n" (("Solucao encontrada:\n" ++) . mostrarTabuleiro) (resolver tab)
  where
    erros = validar tab

-- [PURA] Ação 3: compara a resposta com o original, na mesma ordem de prioridade das outras versões.
relatorioResposta :: Tabuleiro -> Tabuleiro -> String
relatorioResposta original resposta
  | not (estaCompleto resposta) = "Resposta invalida: existem celulas vazias.\n"
  | not (respeitaPistas original resposta) = "Resposta invalida: nao respeita os valores originais do tabuleiro.\n"
  | not (null erros) = "Resposta invalida: ha conflitos.\n" ++ listarErros erros
  | otherwise = "Resposta valida! Parabens.\n"
  where
    erros = validar resposta


-- ============================================================
-- INTERPRETAÇÃO DO ARQUIVO (pura: recebe o texto já lido)
-- ============================================================

-- [PURA] Converte uma linha de texto em 9 dígitos, ou devolve a mensagem de erro.
lerLinha :: Int -> String -> Either String [Int]
lerLinha n s
  | length s /= 9 = Left ("linha " ++ show n ++ " deve conter exatamente 9 caracteres (encontrado " ++ show (length s) ++ ").")
  | all isDigit s = Right (map digitToInt s)
  | otherwise = Left ("caractere invalido na linha " ++ show n ++ ".")

-- [PURA] [ALTA ORDEM] Converte o conteúdo do arquivo num tabuleiro; sequence para no primeiro erro.
-- Remove '\r' para aceitar arquivos salvos no Windows.
interpretarTabuleiro :: String -> Either String Tabuleiro
interpretarTabuleiro conteudo
  | length linhas < 9 = Left "arquivo com menos de 9 linhas."
  | otherwise = sequence (zipWith lerLinha [1 ..] linhas)
  where
    linhas = take 9 (map (filter (/= '\r')) (lines conteudo))


-- ============================================================
-- CAMADA DE ENTRADA E SAÍDA (impura)
-- ============================================================

-- [IMPURA] [MUNDO EXTERNO] Lê o arquivo e delega a interpretação à função pura.
-- O único efeito aqui é a leitura; toda a validação do formato fica no núcleo puro.
lerTabuleiro :: FilePath -> IO (Either String Tabuleiro)
lerTabuleiro caminho = do
  resultado <- try (readFile caminho) :: IO (Either IOException String)
  return (either (const (Left "nao foi possivel abrir o arquivo.")) interpretarTabuleiro resultado)

-- [IMPURA] [MUNDO EXTERNO] Lê a opção do teclado, repetindo (por recursão) até ser 1, 2 ou 3.
lerOpcao :: IO Int
lerOpcao = do
  entrada <- getLine
  case readMaybe entrada of
    Just n | n `elem` [1, 2, 3] -> return n
    _ -> do
      putStr "Acao nao encontrada, tente novamente:"
      hFlush stdout
      lerOpcao

-- [IMPURA] [MUNDO EXTERNO] Executa a ação escolhida: só imprime textos gerados pelas funções puras.
executar :: Int -> Tabuleiro -> IO ()
executar 1 tab = putStr (relatorioValidacao tab)
executar 2 tab = putStr (relatorioResolucao tab)
executar _ tab = do
  resposta <- lerTabuleiro "tabuleiro2.txt"
  putStr (either (\e -> "Erro: " ++ e ++ "\n") (relatorioResposta tab) resposta)

-- [IMPURA] [MUNDO EXTERNO] Ponto de entrada: lê o tabuleiro, mostra o menu e executa a ação.
main :: IO ()
main = do
  leitura <- lerTabuleiro "tabuleiro.txt"
  case leitura of
    Left erro -> putStrLn ("Erro: " ++ erro)
    Right tab -> do
      putStr (mostrarTabuleiro tab)
      putStr "Digite a seguir qual acao queira fazer:\n1-Validar tabuleiro\n2-Resolver tabuleiro\n3-Validar resposta(insira o tabuleiro resolvido no arquivo \"tabuleiro2.txt\")\nDigite aqui:"
      hFlush stdout
      opcao <- lerOpcao
      executar opcao tab