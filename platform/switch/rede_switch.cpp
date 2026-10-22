/**************************************************************************/
/*  rede_switch.cpp                                                       */
/**************************************************************************/
/* Godot 4 para o Nintendo Switch homebrew (libnx). MIT, como o Godot.    */
/**************************************************************************/

// Rede (docs/decisoes.md, D-18): o IP e os sockets são os do drivers/unix (IPUnix, NetSocketUnix) sobre os
// sockets BSD da libnx, ligados pelo OS_Unix::initialize_core. Falta à libnx o getifaddrs: o endereço do console
// vem do nifm (o serviço de rede do sistema), aberto e fechado a cada pedido (nada sobra na saída, D-14).

#include "os_switch.h"

#include "drivers/unix/ip_unix.h"

#include "switch_wrapper.h"

#include <cstdio>
#include <cstring>

// O IPv4 do console na rede (Wi-Fi ou cabo); false sem conexão. O nifm devolve o endereço como o s_addr
// (ordem da rede): o primeiro byte na memória é o primeiro número do endereço.
static bool _endereco_do_console(uint8_t r_ip[4], uint32_t *r_mascara = nullptr) {
	if (R_FAILED(nifmInitialize(NifmServiceType_User))) {
		return false;
	}
	u32 ip = 0;
	u32 mascara = 0;
	u32 roteador = 0;
	u32 dns1 = 0;
	u32 dns2 = 0;
	const Result r = nifmGetCurrentIpConfigInfo(&ip, &mascara, &roteador, &dns1, &dns2);
	nifmExit();
	if (R_FAILED(r) || ip == 0) {
		return false;
	}
	memcpy(r_ip, &ip, 4);
	if (r_mascara) {
		*r_mascara = mascara;
	}
	return true;
}

// O substituto do getifaddrs (o remendo 03-rede-horizon.patch tira o do drivers/unix no Horizon): a volta
// ("lo", 127.0.0.1, como no Linux) e a rede do console ("nifm"), se houver conexão.
void IPUnix::get_local_interfaces(HashMap<String, Interface_Info> *r_interfaces) const {
	Interface_Info volta;
	volta.name = "lo";
	volta.name_friendly = "lo";
	volta.index = "1";
	const uint8_t local[4] = { 127, 0, 0, 1 };
	IPAddress ip_volta;
	ip_volta.set_ipv4(local);
	volta.ip_addresses.push_back(ip_volta);
	r_interfaces->insert(volta.name, volta);

	uint8_t endereco[4];
	if (!_endereco_do_console(endereco)) {
		return;
	}
	Interface_Info rede;
	rede.name = "nifm";
	rede.name_friendly = "nifm";
	rede.index = "2";
	IPAddress ip_rede;
	ip_rede.set_ipv4(endereco);
	rede.ip_addresses.push_back(ip_rede);
	r_interfaces->insert(rede.name, rede);
}

// Uma linha no log da abertura com o estado da rede: "[switch] rede: 192.168.0.7/24 (Wi-Fi, 3 barras)" ou
// "[switch] rede: sem conexão". Antes do Main::setup, com o printf puro. `p_sockets`: o que o
// socketInitializeDefault devolveu (sem os sockets BSD, nada de rede funciona, mesmo com conexão).
void OS_Switch::relatar_rede(uint32_t p_sockets) {
	if (R_FAILED(p_sockets)) {
		printf("[switch] rede: os sockets da libnx não abriram (0x%x): sem rede no jogo\n", p_sockets);
	}
	if (R_FAILED(nifmInitialize(NifmServiceType_User))) {
		printf("[switch] rede: o nifm não abriu (sem informação da rede)\n");
		return;
	}
	NifmInternetConnectionType tipo = (NifmInternetConnectionType)0;
	u32 barras = 0;
	NifmInternetConnectionStatus estado = (NifmInternetConnectionStatus)0;
	const Result r = nifmGetInternetConnectionStatus(&tipo, &barras, &estado);
	nifmExit();
	uint8_t ip[4];
	uint32_t mascara = 0;
	if (!_endereco_do_console(ip, &mascara)) {
		printf("[switch] rede: sem conexão (estado da internet 0x%x)\n", R_FAILED(r) ? r : 0);
		return;
	}
	// a máscara em bits (255.255.255.0 → /24)
	const int bits = __builtin_popcount(mascara);
	if (R_FAILED(r)) {
		// com endereço e sem o estado (rede local sem internet?): o endereço basta para a mesma rede
		printf("[switch] rede: %u.%u.%u.%u/%d (estado da internet indisponível, 0x%x)\n", ip[0], ip[1], ip[2], ip[3], bits, r);
		return;
	}
	const char *meio = tipo == NifmInternetConnectionType_WiFi ? "Wi-Fi" : (tipo == NifmInternetConnectionType_Ethernet ? "cabo" : "?");
	printf("[switch] rede: %u.%u.%u.%u/%d (%s, %u barras, %s)\n", ip[0], ip[1], ip[2], ip[3], bits, meio, barras,
			estado == NifmInternetConnectionStatus_Connected ? "internet ok" : "internet conectando");
}
