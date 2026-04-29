# SIM ROBOTICS Ed. — Word Filter

Ferramenta desktop para busca e análise de arquivos de programação de robôs industriais (FANUC, ABB, etc.). Desenvolvida em C++ com wxWidgets.

---

## Funcionalidades

- **Busca por palavra-chave** em arquivos de programa de robô
- **Três modos de exibição** dos resultados:
  - `Full line` — exibe a linha completa onde o termo foi encontrado
  - `First 2 words` — exibe apenas as duas primeiras palavras da linha
  - `Robtarget XYZ` — extrai e exibe coordenadas de posições ABB (robtarget)
- **Detecção automática de blocos FANUC** — quando encontra uma linha do tipo `P[4:"GSC2B165397"]{`, coleta e exibe o bloco completo até o fechamento `};`
- **Snapshots por aba** — cada busca abre uma nova aba com seus resultados, permitindo comparar múltiplas buscas
- **Comparação entre snapshots** — compara dois resultados e indica quais entradas estão presentes ou ausentes
- **Exportação para CSV** — salva os resultados em arquivo `.csv` separado por ponto e vírgula, com colunas dinâmicas
- **Drag and Drop** — arraste o arquivo diretamente para a janela para carregá-lo

---

## Como usar

### 1. Carregar um arquivo
Clique em **Select File** e escolha o arquivo de programa do robô, ou arraste o arquivo direto para a janela.

### 2. Fazer uma busca
Digite o termo desejado no campo de texto e clique em **Search** (ou pressione Enter).  
Escolha o modo de exibição no seletor ao lado:

| Modo | Descrição |
|---|---|
| Full line | Linha completa. Blocos FANUC `P[N:"..."]{}` são exibidos inteiros. |
| First 2 words | Apenas as duas primeiras palavras de cada linha encontrada. |
| Robtarget XYZ | Extrai nome e coordenadas X, Y, Z de declarações `robtarget` (ABB). |

### 3. Comparar resultados
Vá para a aba **Compare**, selecione dois snapshots nos seletores e clique em **Compare**.  
O resultado mostrará quais entradas estão `OK` (presentes em ambos) ou `MISSING` (ausentes no segundo).

### 4. Exportar para CSV
Após uma busca, clique em **Export CSV** para salvar os resultados.  
O arquivo gerado usa `;` como separador e cada token da linha vira uma coluna separada.

---

## Requisitos

- Sistema operacional: **Windows**
- Biblioteca: **wxWidgets 3.x**
- Compilador: **MinGW / g++** ou **Visual Studio** com suporte a C++17

---

## Compilação

### MinGW / g++

```bash
g++ main.cpp -o app.exe `wx-config --cxxflags --libs` -std=c++17
```

### Com ícone customizado (arquivo .rc)

```bash
windres app.rc -o app_rc.o
g++ main.cpp app_rc.o -o app.exe `wx-config --cxxflags --libs` -std=c++17
```

### CMake

```cmake
cmake_minimum_required(VERSION 3.15)
project(WordFilter)

find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

add_executable(app WIN32 main.cpp app.rc)
target_link_libraries(app ${wxWidgets_LIBRARIES})
target_compile_features(app PRIVATE cxx_std_17)
```

---

## Estrutura do projeto

```
/
├── main.cpp       # Código-fonte principal
├── app.rc         # Resource file do Windows (ícone)
├── ICOS/
│   └── sim.ico    # Ícone da aplicação
└── README.md
```

---

## Formatos de arquivo suportados

O programa lê qualquer arquivo de texto plano. Foi projetado e testado com:

- Arquivos `.ls` de robôs **FANUC**
- Arquivos `.mod` / `.pgf` de robôs **ABB** (RAPID)

---

## Licença

Projeto interno — SIM ROBOTICS.
