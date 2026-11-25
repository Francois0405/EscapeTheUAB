// NetJuego.h
// -----------------------------------------------------------------------------
// Pegamento entre el juego y la capa de red (Network/): ciclo de vida, diagnostico, jugadores remotos,
// puertas con el host como autoridad y el objeto compartido de prueba.
//
// REGLAS
//   * Modo Offline (por defecto): Iniciar() no crea servicio, ni hilo, ni transporte; ENet no se toca nunca.
//   * TODOS los metodos publicos se llaman desde el HILO DE JUEGO. El unico hilo extra de este modulo es el de
//     arranque (resolver DNS / abrir el socket puede tardar y no debe bloquear el render); la red real corre en
//     el hilo de NetService. Los mensajes de red se APLICAN aqui, en Actualizar(), nunca desde otro hilo.
//   * No incluye OpenGL, Bullet ni OpenXR.
// -----------------------------------------------------------------------------
#pragma once

#include "src/networking/replication/NetJuegoLogica.h"
#include "networking/transport/NetService.h"

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

namespace netgame
{
	enum class NetworkPhase { Offline, Arrancando, Activa, Error };

	struct Diagnostico
	{
		NetworkPhase fase = NetworkPhase::Offline;
		std::string modo = "Offline";
		std::string estado = "Sin red";
		std::string mensaje;
		std::string escena;
		std::string endpoint;
		uint16_t puerto = kPuertoPorDefecto;
		net::PeerId peerLocal = net::kUnassignedPeerId;
		size_t remotos = 0;
		size_t eventosDescartados = 0;   // cola de eventos llena
		size_t eventosIgnorados = 0;     // invalidos / fuera de turno / rechazados por el host
		size_t puertasEnRed = 0;
	};

	class NetworkGame
	{
	public:
		using Fabrica = std::function<std::unique_ptr<net::ITransport>(const NetworkConfig&)>;
		// 'fabrica' = nullptr usa ENet (si esta compilado). Los tests inyectan un transporte en memoria.
		explicit NetworkGame(Fabrica fabrica = nullptr);
		~NetworkGame();
		NetworkGame(const NetworkGame&) = delete;
		NetworkGame& operator=(const NetworkGame&) = delete;

		// Offline: devuelve true sin hacer nada. 'puertas' debe sobrevivir a la sesion (o llamar a Detener antes).
		// 'posObjetoCompartido' solo lo usa el host: donde nace el objeto de prueba.
		bool Iniciar(const NetworkConfig& cfg, const std::string& escenaId, ISceneDoors* puertas, const glm::vec3& posObjetoCompartido);
		void Detener();
		void PuertasCambiadas();   // tras recargar la escena: reconstruye la tabla de IDs

		// Una vez por fotograma. 'poseLocal' = pose del jugador local (nullptr si no hay): solo se LEE.
		void Actualizar(double ahora, const net::PoseEntry* poseLocal);
		// Registro de estado de escena: se llama al iniciar red y al confirmar una edicion del host.
		bool FijarEstadoEscena(const std::vector<SceneObjectState>& estado, bool difundir);
		void TomarEstadosEscena(std::vector<SceneObjectState>& salida);
		void EnviarEstadosFisicos(const std::vector<net::DynamicStateEntry>& estados);
		std::map<uint32_t, net::DynamicStateEntry> EstadosFisicosRemotos() const;
		void TomarEstadosFisicosFiables(std::vector<net::DynamicStateEntry>& salida);
		bool PublicarEstadoFisicoFinal(const net::DynamicStateEntry& estado, net::PeerId soloPara = net::kUnassignedPeerId);
		void TomarSolicitudesFisicas(std::vector<DynamicObjectRequest>& salida);
		bool SolicitarAgarreDinamico(uint32_t id, const glm::vec3& origen, const glm::vec3& direccion);
		bool SolicitarSoltarDinamico(uint32_t id, const glm::vec3& velocidad);
		void FijarPropietarioDinamico(uint32_t id, net::PeerId propietario);
		net::PeerId PropietarioDinamico(uint32_t id) const;
		std::map<uint32_t, net::PeerId> PropietariosDinamicos() const { return propietariosDinamicos_; }
		uint32_t ObjetoDinamicoDePeer(net::PeerId peer) const;
		bool PoseDePeer(net::PeerId peer, net::PoseEntry& salida) const;

		bool Activa() const { return fase_ == NetworkPhase::Activa; }
		bool Conectando() const { return fase_ == NetworkPhase::Arrancando; }
		bool EnRed() const { return fase_ == NetworkPhase::Activa || fase_ == NetworkPhase::Arrancando; }
		bool EsHost() const { return fase_ != NetworkPhase::Offline && cfg_.rol == net::Role::Host; }
		bool EsCliente() const { return fase_ != NetworkPhase::Offline && cfg_.rol == net::Role::Client; }
		net::PeerId PeerLocal() const { return servicio_ ? servicio_->LocalPeer() : net::kUnassignedPeerId; }
		Diagnostico ObtenerDiagnostico() const;
		static bool EnetDisponible();   // construye (sin inicializar) un transporte ENet de prueba

		// ---- puertas ----
		// true = la red se ocupa (cliente: solicitud enviada al host, NO se toca la puerta localmente);
		// false = el llamante debe alternarla (offline o host; el host la difunde solo).
		bool InterceptarPuerta(size_t indicePuerta);

		// ---- objeto compartido de prueba ----
		const SharedObjectView& Objeto() const { return vista_; }
		glm::vec3 PosicionVisualObjeto() const;
		bool SoyDuenoDelObjeto() const { return vista_.conocido && vista_.dueno != kSinDueno && vista_.dueno == PeerLocal(); }
		// Pide/suelta el objeto si el rayo lo apunta al alcance (o si ya es mio, lo suelta). true si lo gestiono.
		bool AlternarObjeto(const glm::vec3& origen, const glm::vec3& direccion, double ahora);
		void ActualizarAcarreo(const glm::vec3& origen, const glm::vec3& direccion, double ahora);

		// ---- jugadores remotos (ya interpolados; NUNCA incluye al jugador local) ----
		std::vector<net::PoseEntry> RemotosParaDibujar(double ahora) const;

	private:
		void Procesar(const net::NetEvent& e, double ahora, const std::map<net::PeerId, net::PoseEntry>& poses);
		void ProcesarHost(const net::NetEvent& e, double ahora, const std::map<net::PeerId, net::PoseEntry>& poses);
		void ProcesarCliente(const net::NetEvent& e);
		void DifundirHost(double ahora);
		void ReiniciarEstadoRemoto();
		void Fallar(const std::string& msg);
		bool Encolar(const OutputEvent& e, net::PeerId soloPara = net::kUnassignedPeerId);
		bool Cabeza(net::PeerId p, const std::map<net::PeerId, net::PoseEntry>& poses, glm::vec3& out) const;

		Fabrica fabrica_;
		NetworkConfig cfg_;
		NetworkPhase fase_ = NetworkPhase::Offline;
		std::string escena_, mensaje_;
		std::unique_ptr<net::NetService> servicio_;
		std::thread arranque_;
		std::atomic<bool> arrancando_{ false }, arranqueOk_{ false }, sinTransporte_{ false };
		bool pendienteDetener_ = false;

		ISceneDoors* puertas_ = nullptr;
		DoorSync sincroPuertas_;
		OwnershipArbiter arbitro_;
		SharedObjectView vista_;
		glm::vec3 posVisual_{ 0 };
		glm::vec3 posObjetoInicial_{ 0 };
		RemoteInterpolator interp_;

		net::PoseEntry poseLocal_; bool hayPoseLocal_ = false;
		double ultimaSubida_ = -1e9, ultimoAhora_ = 0, ultimoAcarreo_ = -1e9;
		glm::vec3 ultimoPosAcarreo_{ 0 }, destinoAcarreo_{ 0 };
		std::vector<net::StateEntry> recuperacionEnviada_;
		std::vector<SceneObjectState> estadoEscena_;
		std::vector<SceneObjectState> estadosRecibidos_;
		std::map<uint32_t, net::DynamicStateEntry> estadosFisicosHost_;
		std::vector<net::DynamicStateEntry> estadosFisicosFiables_;
		std::vector<DynamicObjectRequest> solicitudesFisicas_;
		std::map<uint32_t, net::PeerId> propietariosDinamicos_;
		bool recuperacionSucia_ = true;
		net::ConnectionState estadoConexion_ = net::ConnectionState::Idle;
		size_t ignorados_ = 0;
		std::string errorPuertas_;
	};
}
