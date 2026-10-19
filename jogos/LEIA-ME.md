# jogos/

Configurações de jogos específicos, uma pasta por jogo (`jogos/<jogo>/jogo.cfg`), cada uma numa branch
com o nome do jogo. Assim a main fica genérica e o projeto do jogo não precisa ter nada do Switch.

O `gsn.py gerar <projeto>` acha a configuração sozinho pelo `projeto_nome` (o `application/config/name`
do projeto). As chaves estão no README ("A configuração do jogo"). Ao lado do `jogo.cfg` podem ficar o
ícone, o arquivo de versão, ferramentas e a documentação do jogo no Switch.

Branches de jogos:

- `batida-capixaba`: o Batida Capixaba, o jogo para o qual o port foi escrito.
