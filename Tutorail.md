# Sequência do Envio de algo por via (...)
## Important stuff
1. Flag -> Indica o início e o fim dos frames, o receptor já sabe.
2. Address -> Identifica quem é o destino da frame (em sistemas multiponto). Em ligações ponto-a-ponto, normalmente é fixo.
3. Control -> Indica o tipo de frame (dados, ACK, NACK, setup, etc.) e pode também transportar o número de sequência do frame.
4. BCC -> Código de verificação gerado a partir dos bits do frame, server para detectar erros de transmissão.
## Transmitter

### Application Layer
- Lê arquivo do disco
- Divide em blocos/
- Chama funções da Link Layer (API) para enviar cada bloco
### Link Layer
- Recebe cada bloco da Application Layer
- Monta frames (FLAG, ADDRESS, CONTROL, DATA, BCC, FLAG) a partir dos blocos
- Envia frame pela porta serial (TX -> RS-232 )
- Espera ACK do receptor
## Receiver

### Link Layer
- Recebe frame da porta serial
- Verifica FLAG, BCC, CONTROL
- Envia ACK/NACK para transmissor
	`Poderia envirar NACK se`
	1. Se o flag está errado indicando que o frame veio incompleto
    2. BCC errado, mostrando que houve erro de transmissão( bits alterados no caminho )
    3. Control indica qual número de frame está chegando, se for repetido foi reenvio
- Passa DATA( blocos limpos ) para Application Layer
### Application Layer
- Recebe blocos da Link Layer
- Junta blocos para reconstruir arquivo
- Salva arquivo no disco

---

# Fluxo de Frames e Comunicação RS-232

## 1. Início da comunicação
1. **Transmissor (TX)** envia **SET (S/U Frame)**
   - Control: 0x03
   - Função: Inicia a conexão com o receptor
2. **Receptor (RX)** recebe SET e envia **UA (S/U Frame)**
   - Control: 0x07
   - Função: Confirma que a conexão foi iniciada

---

## 2. Envio de dados
> O transmissor envia **I-Frames** contendo blocos de dados, numerados alternadamente 0 e 1. ( não quer dizer que só tenham 2 frames, isso é usado para saber se houve repitição de frams )M

1. **Transmissor (TX)** envia **I-Frame nº0 ou nº1**
   - Control: 0x00 (frame 0) / 0x80 (frame 1)
   - DATA: bloco do arquivo
   - BCC1: protege cabeçalho (Address XOR Control)
   - BCC2: protege dados (XOR de todos os bytes do DATA)
2. **Receptor (RX)** verifica o frame
   - Checa FLAG, BCC1, BCC2 e número de sequência
   - Se **tudo certo** → envia **RR0 / RR1 (ACK)**
     - Control: 0xAA / 0xAB
     - Função: “Recebi frame X corretamente, pronto para o próximo”
   - Se **houve erro** → envia **REJ0 / REJ1 (NACK)**
     - Control: 0x54 / 0x55
     - Função: “Erro no frame X, precisa reenviar”
3. **Transmissor** aguarda ACK/NACK
   - Se ACK → envia próximo I-Frame (número alternado)
   - Se NACK → reenvia mesmo I-Frame

> **Observação:** Pode haver **vários I-Frames** sequenciais, cada um com um bloco do arquivo.

---

## 3. Final da comunicação
1. **Transmissor (TX)** envia **DISC (S/U Frame)**
   - Control: 0x0B
   - Função: Indica que a transmissão terminou
2. **Receptor (RX)** pode responder DISC
   - Função: Confirma o encerramento da comunicação
   
TX -> DISC   (solicita encerramento)
RX -> DISC   (confirma recebimento, pronto para encerrar)
TX -> UA     (confirma que recebeu o DISC do RX)

---

## 4. Resumo rápido dos tipos de frames

| Tipo de Frame | Quem envia | Quando usado | Control | Função |
|---------------|-----------|-------------|--------|--------|
| I-Frame       | Transmissor | Para enviar dados | 0x00 / 0x80 | Transporta dados da aplicação |
| SET           | Transmissor | Início da conexão | 0x03 | Inicia conexão |
| DISC          | Transmissor | Fim da transmissão | 0x0B | Finaliza conexão |
| UA            | Receptor   | Recebe SET | 0x07 | Confirma conexão |
| RR0 / RR1     | Receptor   | Recebe I-Frame correto | 0xAA / 0xAB | Confirmação (ACK) |
| REJ0 / REJ1   | Receptor   | Recebe I-Frame com erro | 0x54 / 0x55 | Rejeição, solicita reenvio (NACK) |

---

## 5. Dicas rápidas
- I-Frames → dados da aplicação  
- S/U Frames → controle e supervisão (SET, UA, DISC, RR, REJ)  
- Control → indica tipo de frame e número de sequência  
- BCC1 → protege cabeçalho (Address + Control)  
- BCC2 → protege dados (somente I-Frames)  
- Pode haver **vários I-Frames** em sequência, cada um com bloco diferente do arquivo