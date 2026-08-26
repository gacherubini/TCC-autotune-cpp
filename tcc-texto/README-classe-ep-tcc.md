EP-TCC
=========

Classe LaTeX + layout LyX e documentos de exemplo para Trabalhos de Conclusão na Escola Politécnica - PUCRS

Adaptado da classe PP-LaTeX criada por Ricardo Piccoli <https://github.com/piccoli/PP-LaTeX>

Para instruções de como utilizar a classe, veja o arquivo
`exemplo.tex`. Para facilitar, recomendamos utilizar o mesmo
arquivo como template inicial para o seu trabalho.

**ATENÇÃO!** O modelo implementado por esta classe NÃO é oficial da
PUCRS e não implementa todas as normas de formatação propostas
pela Biblioteca Central da PUCRS.

**IMPORTANTE**: examine os arquivos 'makefile' e 'sort.sh' antes de
utilizá-los para compilar o seu documento, pois a
responsabilidade pelo uso destes é inteiramente do usuário.

## Requisitos

É recomendado a instalação da versão mais recente da distribuição TeXLive (para Windows e UNIXes):
<http://www.tug.org/texlive/>. Nas distribuições Linux, os nomes dos pacotes necessários são:

- texlive-basic
- texlive-fontsrecommended
- texlive-langenglish
- texlive-langportuguese
- texlive-latex
- texlive-latexextra
- texlive-latexrecommended
- xcolor

Para a distribuição Ubuntu Linux, provavelmente o mais fácil é
instalar o pacote 'texlive-full'.

## Limitações

- Funciona apenas em dois idiomas: Português e Inglês
- Formata PEP's, monografias, teses propostas de tese, propostas e relatórios de TC (I e II)
- A ordenação automática da lista de siglas/abreviaturas ainda
  requer um utilitário externo (`sort.sh`). Porém, para
  simplificar a classe é desejável uma implementação interna
  deste procedimento.

Obs.: este modelo foi criado inicialmente apenas para suprir as
necessidades do autor original (Ricardo) quanto à conformidade ao formato do
PPGCC/PUCRS (especialmente o estilo bibliográfico).

Caso lhe falte alguma formatação em particular do padrão, ou porventura
tenha sugestões, comentários, ou ainda, tenha encontrado um erro
no formato, por favor entre em contato com o autor pelo e-mail
<rfbpiccoli at gmail dot com> (Ricardo Piccoli).

Para sugestões e/ou correções no estilo para **Trabalho de Conclusão**, entre em contato
com Marcelo Cohen <marcelo dot cohen at pucrs dot br>.

