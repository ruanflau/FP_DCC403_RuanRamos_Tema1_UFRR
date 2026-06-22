# FP_DCC403_RuanRamos_Tema1_UFRR

**Universidade Federal de Roraima — UFRR**  
**Departamento de Ciência da Computação**  
**DCC403 - Sistemas Operacionais I — T01**  
**Professor:** Herbert Oliveira Rocha  
**Aluno:** Ruan Felipe Lauriano Ramos  
**Tema 1:** Desenvolvimento de um Ecossistema de Comunicação Segura e Otimizada para Redes Industriais em Ambientes de Edge Computing

---

## Descrição do Projeto

Este projeto implementa um ecossistema de comunicação cliente-servidor para redes industriais em ambientes de Edge Computing. O sistema é composto por dois binários:

- **`industrial_agent`** — servidor que roda na borda industrial, expondo um shell remoto (`/bin/bash`) de forma transparente e segura via socket TCP.
- **`control_proxy`** — cliente que permite ao engenheiro de automação operar remotamente o shell do agente, com suporte a criptografia TLS, compressão de dados e log de auditoria.

A comunicação suporta ativação modular de **compressão via zlib** (`--compress`) e **criptografia via OpenSSL TLS 1.3** (`--encrypt`), podendo ser usadas de forma isolada ou combinada.

---

## Estrutura do Repositório

```
FP_DCC403_RuanRamos_Tema1_UFRR/
│
├── src/
│   ├── agent.c          # Industrial Edge Agent (servidor)
│   ├── proxy.c          # Central Control Proxy (cliente)
│   ├── common.h         # Constantes globais e struct AppConfig
│   ├── common.c         # Parser de argumentos CLI
│   ├── compress.c/h     # Módulo de compressão (zlib)
│   ├── tls.c/h          # Módulo de criptografia (OpenSSL)
│   ├── logger.c/h       # Módulo de logging de auditoria
│   ├── net_io.c/h       # Camada unificada de I/O de rede
│   └── net_io.h
│
├── certs/
│   └── server.crt       # Certificado TLS autoassinado (gerado localmente)
│
├── logs/
│   └── telemetria_auditoria.log  # Exemplo de log de auditoria gerado em teste
│
├── resultados/          # Evidências dos testes da Fase 4
│   ├── 4.1_teste_basico.txt
│   ├── 4.2_teste_compress.txt
│   ├── 4.3_teste_encrypt.txt
│   ├── 4.3_handshake_tls.txt
│   ├── 4.5_antes_ctrlc.txt
│   ├── 4.5_depois_ctrlc.txt
│   ├── 4.6_multiplos_clientes.txt
│   ├── 4.7_valgrind_agent.txt
│   └── 4.7_valgrind_proxy.txt
│
├── docs/
│   └── relatorio.pdf    # Artigo no padrão IEEE
│
├── Makefile
├── .gitignore
└── README.md
```

---

## Dependências

```bash
sudo apt install build-essential libssl-dev zlib1g-dev valgrind
```

---

## Compilação

```bash
make all
```

Para limpar os binários e objetos:

```bash
make clean
```

Para verificar vazamentos de memória via valgrind:

```bash
make valgrind
```

---

## Geração dos Certificados TLS

Os certificados **não estão versionados** no repositório por boas práticas de segurança. Cada desenvolvedor deve gerá-los localmente antes de compilar:

```bash
openssl req -x509 -newkey rsa:4096 \
  -keyout certs/server.key \
  -out certs/server.crt \
  -days 365 -nodes
```

Preencha os campos conforme desejado. No campo **Common Name**, use `localhost` para testes locais.

---

## Execução

### Modo básico (sem flags)

**Terminal 1 — Servidor:**

```bash
./industrial_agent --port=9095
```

**Terminal 2 — Cliente:**

```bash
./control_proxy --host=127.0.0.1 --port=9095
```

### Com compressão

```bash
./industrial_agent --port=9095 --compress
./control_proxy --host=127.0.0.1 --port=9095 --compress
```

### Com criptografia TLS

```bash
./industrial_agent --port=9095 --encrypt
./control_proxy --host=127.0.0.1 --port=9095 --encrypt
```

### Modo completo (compressão + TLS + log de auditoria)

```bash
./industrial_agent --port=9095 --compress --encrypt

./control_proxy --host=127.0.0.1 --port=9095 \
  --compress --encrypt \
  --log=logs/telemetria_auditoria.log
```

---

## Argumentos Disponíveis

| Argumento           | Binário | Descrição                                                 |
| ------------------- | ------- | --------------------------------------------------------- |
| `--port=NUMERO`     | ambos   | Porta TCP (servidor: escuta, cliente: conecta)            |
| `--host=ENDERECO`   | proxy   | Endereço IP do servidor                                   |
| `--compress`        | ambos   | Ativa compressão zlib no pipeline de dados                |
| `--encrypt`         | ambos   | Ativa camada TLS via OpenSSL                              |
| `--log=arquivo.log` | proxy   | Grava log de auditoria no formato `SENT/RECEIVED N bytes` |

---

## Formato do Log de Auditoria

Quando `--log=` está ativo, cada transação é registrada no seguinte formato:

```
SENT 9 bytes: uname -a
RECEIVED 512 bytes: Linux LauriComputador 6.17.0-35-generic ...
```

---

## Controle Remoto de Sinais

O `control_proxy` opera o terminal em **modo raw**, interceptando os bytes de controle antes que o sistema operacional os processe localmente:

- **`Ctrl+C`** — enviado como byte `0x03` pela rede; o agente traduz em `SIGINT` para o processo shell filho via `killpg()`.
- **`Ctrl+D`** — enviado como byte `0x04`; o agente fecha o pipe de stdin do shell, sinalizando EOF de forma ordenada.

---

## Limitações Conhecidas

- **`--compress` com payloads muito grandes:** o protocolo atual não implementa framing de mensagens (length-prefixing). Em transmissões de arquivos muito grandes (ex: `cat /var/log/syslog`), o TCP pode fragmentar os blocos comprimidos de forma que o `inflate()` do receptor falhe. O sistema funciona corretamente para comandos de saída moderada.

- **Certificado autoassinado:** o certificado gerado localmente não pertence a uma CA reconhecida. O `control_proxy` desativa a verificação estrita (`SSL_VERIFY_NONE`) por design, adequado para o ambiente de testes acadêmico. Em produção, um certificado assinado por CA confiável deveria ser utilizado, com verificação ativa.

- **`server.key` não versionado:** a chave privada TLS é ignorada pelo `.gitignore` e deve ser gerada localmente por cada desenvolvedor conforme instruções acima.

---

## Resultados de Validação

| Teste | Descrição                             | Resultado                               |
| ----- | ------------------------------------- | --------------------------------------- |
| 4.1   | Conexão básica sem flags              | ✅ Aprovado                             |
| 4.2   | Compressão `--compress`               | ✅ Aprovado (limitação documentada)     |
| 4.3   | Criptografia `--encrypt`              | ✅ Aprovado (TLS 1.3 / AES-256-GCM)     |
| 4.4   | Flags completas + log de auditoria    | ✅ Aprovado                             |
| 4.5   | `Ctrl+C` remoto interrompendo `sleep` | ✅ Aprovado                             |
| 4.6   | Múltiplos clientes simultâneos        | ✅ Aprovado                             |
| 4.7   | Valgrind — ausência de memory leaks   | ✅ `0 errors, definitely lost: 0 bytes` |

---

## Referências

- KERRISK, Michael. _The Linux Programming Interface_. No Starch Press, 2010.
- STEVENS, W. Richard; RAGO, Stephen A. _Advanced Programming in the UNIX Environment_. Addison-Wesley, 3ª ed., 2013.
- ZLIB DEVELOPMENT TEAM. _Zlib Usage Manual and API Reference_. Disponível em: https://www.zlib.net/zlib_how.html
- OPENSSL FOUNDATION. _OpenSSL API Documentation / SSL Layer_. Disponível em: https://wiki.openssl.org/index.php/SSL/TLS_Client
