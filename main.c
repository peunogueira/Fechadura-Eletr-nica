#include <avr/io.h>
#include <util/delay.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "lcd/i2c.h"
#include "lcd/LCD_I2C.h"
#include "DS1307/ds1307.h"
#include "uart/uart.h"
#include <avr/eeprom.h>

#define  F_CPU 8000000UL


// Defini??es para o LCD
#define RS PD2
#define E  PD3
#define DATA_PORT PORTD
#define CTRL_PORT PORTD
#define DATA_DDR DDRD
#define CTRL_DDR DDRD
#define LED_VERDE    PB7
#define LED_VERMELHO PD0

// Definindo o teclado com as novas configura??es de pinos
#define TECLADO_PORTC PORTC    // Porta C para as linhas
#define TECLADO_DDRC  DDRC     // DDR para Porta C (linhas)
#define TECLADO_PINC  PINC     // PIN para Porta C (linhas)

#define TECLADO_PORTB PORTB    // Porta B para as colunas
#define TECLADO_DDRB  DDRB     // DDR para Porta B (colunas)
#define TECLADO_PINB  PINB     // PIN para Porta B (colunas)

// Defini??es para EEPROM
#define EEPROM_USUARIOS_BASE 0
#define TAM_USUARIO 10
#define TAM_SENHA 10
#define MAX_USUARIOS 5

void inicializaLEDs() {
	DDRD |= (1 << LED_VERMELHO);  // Configura PC3 (vermelho) como saída
	PORTD &= ~( (1 << LED_VERMELHO));  // Garante que o LED vermelho
	DDRB |= (1 << LED_VERDE);  // Configura PB7 (verde)como saída
	PORTB &= ~((1 << LED_VERDE)); // Garante que o LED verde
}

// Fun??o para acender o LED verde
void acendeLEDVerde() {
	PORTB |= (1 << LED_VERDE);  // Acende o LED verde
	PORTD &= ~(1 << LED_VERMELHO);  // Apaga o LED vermelho
}

// Fun??o para acender o LED vermelho
void acendeLEDVermelho() {
	PORTD |= (1 << LED_VERMELHO);  // Acende o LED vermelho
	PORTB &= ~(1 << LED_VERDE);  // Apaga o LED verde
}

// Fun??o para apagar os LEDs
void apagaLEDs() {
	PORTD &= ~((1 << LED_VERMELHO));  // Apaga ambos os LEDs
	PORTB &= ~((1 << LED_VERDE));
}

// Fun??es para o LCD
void enviaComando(unsigned char comando) {
	DATA_PORT = (DATA_PORT & 0x0F) | (comando & 0xF0);
	CTRL_PORT &= ~(1 << RS);
	CTRL_PORT |= (1 << E);
	_delay_us(1);
	CTRL_PORT &= ~(1 << E);
	_delay_us(200);
	DATA_PORT = (DATA_PORT & 0x0F) | (comando << 4);
	CTRL_PORT |= (1 << E);
	_delay_us(1);
	CTRL_PORT &= ~(1 << E);
	_delay_ms(2);
}

void enviaDados(unsigned char dado) {
	DATA_PORT = (DATA_PORT & 0x0F) | (dado & 0xF0);
	CTRL_PORT |= (1 << RS);
	CTRL_PORT |= (1 << E);
	_delay_us(1);
	CTRL_PORT &= ~(1 << E);
	_delay_us(200);
	DATA_PORT = (DATA_PORT & 0x0F) | (dado << 4);
	CTRL_PORT |= (1 << E);
	_delay_us(1);
	CTRL_PORT &= ~(1 << E);
	_delay_ms(2);
}

void inicializaLCD() {
	DATA_DDR |= 0xF0;
	CTRL_DDR |= (1 << RS) | (1 << E);
	_delay_ms(20);
	enviaComando(0x02);
	enviaComando(0x28);
	enviaComando(0x0C);
	enviaComando(0x06);
	enviaComando(0x01);
	_delay_ms(2);
}

void exibeString(const char *str) {
	while (*str) {
		enviaDados(*str++);
	}
}

// Fun??es para o teclado matricial
void inicializaTeclado() {
	// Configurar as linhas (PC0 a PC3) como sa?da e colunas (PB0, PB1 e PB3) como entrada
	TECLADO_DDRC = 0x0F;   // 0000 1111 - PC0, PC1, PC2, PC3 como sa?da (linhas)
	TECLADO_PORTC = 0x00;  // Inicialmente as linhas em 0 (ativa as linhas)

	TECLADO_DDRB = 0x00;   // 0000 0000 - PB0, PB1 e PB3 como entrada (colunas)
	TECLADO_PORTB = 0x0F;   // 0000 1111 - Ativa pull-up nas colunas PB0, PB1 e PB3
}

char verificaTeclado() {
	char teclas[4][3] = {
		{'1', '2', '3'},
		{'4', '5', '6'},
		{'7', '8', '9'},
		{'*', '0', '#'}
	};

	for (uint8_t linha = 0; linha < 4; linha++) {
		TECLADO_PORTC = ~(1 << linha); // Ativar a linha correspondente (PC0 a PC3)
		_delay_us(5); // Debounce

		for (uint8_t coluna = 0; coluna < 3; coluna++) {
			if (!(TECLADO_PINB & (1 << coluna))) { // Verificar se a tecla foi pressionada (coluna)
				_delay_ms(50); // Debounce
				while (!(TECLADO_PINB & (1 << coluna))); // Esperar at? que a tecla seja liberada
				return teclas[linha][coluna]; // Retorna o caractere pressionado
			}
		}
	}
	return '\0'; // Nenhuma tecla pressionada
}

// Fun??es para manipula??o da EEPROM
void salvarUsuarioNaEEPROM(uint8_t index, const char *usuario, const char *senha) {
	uint16_t endereco = EEPROM_USUARIOS_BASE + (index * (TAM_USUARIO + TAM_SENHA));
	eeprom_update_block((const void *)usuario, (void *)endereco, TAM_USUARIO);
	eeprom_update_block((const void *)senha, (void *)(endereco + TAM_USUARIO), TAM_SENHA);
}

void carregarUsuarioDaEEPROM(uint8_t index, char *usuario, char *senha) {
	uint16_t endereco = EEPROM_USUARIOS_BASE + (index * (TAM_USUARIO + TAM_SENHA));
	eeprom_read_block((void *)usuario, (const void *)endereco, TAM_USUARIO);
	eeprom_read_block((void *)senha, (const void *)(endereco + TAM_USUARIO), TAM_SENHA);
}

void inicializaUsuariosEEPROM() {
	static char usuarioInicial[TAM_USUARIO] = "00";
	static char senhaInicial[TAM_SENHA] = "1897";
	salvarUsuarioNaEEPROM(0, usuarioInicial, senhaInicial);
}

// Fun??es de gerenciamento de usu?rios
void adicionarUsuario(uint8_t *numUsuarios) {
	if (*numUsuarios >= MAX_USUARIOS) {
		enviaComando(0x01);
		exibeString("Limite Atingido");
		_delay_ms(2000);
		return;
	}

	char novoUsuario[TAM_USUARIO] = "";
	char novaSenha[TAM_SENHA] = "";
	char tecla;
	uint8_t pos = 0;

	// Entrada do novo nome de usuário
	enviaComando(0x01);
	exibeString("Novo Usuario:");
	enviaComando(0xC0);
    


	while (1) {
		tecla = verificaTeclado();
		if (tecla == '#') break;
        if (tecla == '*') break;
		if (tecla != '\0' && pos < TAM_USUARIO - 1) {
			novoUsuario[pos++] = tecla;
			novoUsuario[pos] = '\0';
			enviaDados(tecla);
		}
	}

	// Verificar se o nome de usuário já existe
	for (uint8_t i = 0; i < *numUsuarios; i++) {
		char usuarioExistente[TAM_USUARIO];
		char senhaExistente[TAM_SENHA];
		carregarUsuarioDaEEPROM(i, usuarioExistente, senhaExistente);

		if (strcmp(novoUsuario, usuarioExistente) == 0) {
			enviaComando(0x01);
			exibeString("Usuario Existe");
			_delay_ms(5000);
			adicionarUsuario(numUsuarios);
			return;
		}
	}

	// Entrada da nova senha
	enviaComando(0x01);
	exibeString("Nova Senha:");
	enviaComando(0xC0);

	pos = 0;

	while (1) {
		tecla = verificaTeclado();
		if (tecla == '#') break;
		if (tecla != '\0' && pos < TAM_SENHA - 1) {
			novaSenha[pos++] = tecla;
			novaSenha[pos] = '\0';
			enviaDados('*');
		}
	}

	// Salvar o novo usuário
	salvarUsuarioNaEEPROM(*numUsuarios, novoUsuario, novaSenha);
	(*numUsuarios)++;
	enviaComando(0x01);
	exibeString("Usuario Salvo");
	_delay_ms(2000);
}


int verificarCredenciais(const char *usuarioInserido, const char *senhaInserida, uint8_t numUsuarios) {
	char usuarioEEPROM[TAM_USUARIO];
	char senhaEEPROM[TAM_SENHA];

	for (uint8_t i = 0; i < numUsuarios; i++) {
		carregarUsuarioDaEEPROM(i, usuarioEEPROM, senhaEEPROM);
		if (strcmp(usuarioInserido, usuarioEEPROM) == 0 &&
		strcmp(senhaInserida, senhaEEPROM) == 0) {
			return 1;
		}
	}
	return 0;
}

// Programa principal
// Programa principal
int main() {
	I2C_Init();
	//Inicializando o uart [TERMINAL]
	UART_Init(51);
	//Inicializando SD card

	inicializaLCD();
	inicializaTeclado();
	inicializaUsuariosEEPROM();
	inicializaLEDs();

	uint8_t numUsuarios = 1;
	uint8_t tentativas = 0;  // Contador de tentativas consecutivas incorretas

	while (1) {
		char usuarioInserido[TAM_USUARIO] = "";
		char senhaInserida[TAM_SENHA] = "";
		char tecla;
		uint8_t pos;

		enviaComando(0x01);
		exibeString("Usuario:");
		enviaComando(0xC0);

		pos = 0

		while (1) {
			tecla = verificaTeclado();
			if (tecla == '#') break;
			if (tecla != '\0' && pos < TAM_USUARIO - 1) {
				usuarioInserido[pos++] = tecla;
				usuarioInserido[pos] = '\0';
				enviaDados(tecla);
			}
		}
        

		enviaComando(0x01);
		exibeString("Senha:");
		enviaComando(0xC0);

		pos = 0;

            
		while (1) {
			tecla = verificaTeclado();
			if (tecla == '#') break;
			if (tecla != '\0' && pos < TAM_SENHA - 1) {
				senhaInserida[pos++] = tecla;
				senhaInserida[pos] = '\0';
				enviaDados('*');
			}
		}

		if (verificarCredenciais(usuarioInserido, senhaInserida, numUsuarios)) {
			tentativas = 0; // Reseta o contador de tentativas incorretas

			if (strcmp(usuarioInserido, "00") == 0 && strcmp(senhaInserida, "1897") == 0) {
				
				adicionarUsuario(&numUsuarios);
				
				} else {
				RTC_Read_Clock();
				UART_SendString(usuarioInserido, HORA_DS);
				enviaComando(0x01);
				exibeString("Bem vindo!");
				acendeLEDVerde();  // Acende o LED verde para sucesso
				_delay_ms(2000);
				apagaLEDs();
			}
			} else {
			tentativas++; // Incrementa o contador de tentativas incorretas

			if (tentativas >= 3) {
				enviaComando(0x01);
				exibeString("Bloqueado!");
				acendeLEDVermelho();  // Acende o LED vermelho
				_delay_ms(2000);

				// Requer senha do usu?rio administrador (00) para desbloqueio
				while (1) {
					char senhaAdmin[TAM_SENHA] = "";
					uint8_t posAdmin = 0;

					enviaComando(0x01);
					exibeString("Senha admin:");
					enviaComando(0xC0);
                    

					while (1) {
						tecla = verificaTeclado();
						if (tecla == '#') break;
						if (tecla != '\0' && posAdmin < TAM_SENHA - 1) {
							senhaAdmin[posAdmin++] = tecla;
							senhaAdmin[posAdmin] = '\0';
							enviaDados('*');
						}
					}

					// Verifica a senha do administrador
					char usuarioAdmin[TAM_USUARIO];
					char senhaAdminEEPROM[TAM_SENHA];
					carregarUsuarioDaEEPROM(0, usuarioAdmin, senhaAdminEEPROM);

					if (strcmp(senhaAdmin, senhaAdminEEPROM) == 0) {
						tentativas = 0; // Reseta o contador ao desbloquear
						enviaComando(0x01);
						exibeString("Desbloqueado!");
						acendeLEDVerde();
						_delay_ms(2000);
						apagaLEDs();
						break;
						} else {
						enviaComando(0x01);
						exibeString("Senha incorreta!");
						acendeLEDVermelho();
						_delay_ms(2000);
						apagaLEDs();
						if (tentativas >= 3){
							acendeLEDVermelho();
						}
					}
				}
				} else {
				enviaComando(0x01);
				exibeString("Senha incorreta!");
				acendeLEDVermelho();  // Acende o LED vermelho
				_delay_ms(2000);
				apagaLEDs();
			}
		}
	}

	return 0;
}
