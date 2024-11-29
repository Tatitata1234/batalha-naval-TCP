apt-get install libcjson-dev
gcc teste.c -o teste -lcjson
./teste

O jogo vai persistir o json no arquivo dados.json.

Nesse repositório tem 2 arquivos json, simulando 
o board de cada jogador, então para conseguir ter 
a persisitencia inicialmente pode adicionar esses 
arquivos de cada lado para ter um jogo ja iniciado,
apenas precisa mudar o nome do arquivo. 

A cada jogada feita ou recebida a aplicação salva o 
estado atual no dados.json. Se houver um erro o 
arquivo vai estar atualizado.
