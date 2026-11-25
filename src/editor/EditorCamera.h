// CamaraEditor.h
// -----------------------------------------------------------------------------
// Cámara del editor (estilo Unity) y movimiento de objetos con flechas. Solo GLM: se prueba sin ventana.
//
// CONVENIOS (Z arriba)
//   yaw   : 0 mira hacia +X; crece en sentido antihorario visto desde arriba (hacia +Y, la IZQUIERDA del que mira).
//   pitch : positivo mira hacia arriba.
//   adelante = (cos p cos y, cos p sin y, sin p);  derecha = adelante x Z = (sin y, -cos y, 0);  arriba = derecha x adelante.
//
// RATON (el signo se deduce de estos convenios, no se invierte "a ojo"):
//   mover a la DERECHA  (dx > 0) -> mirar a la derecha  -> yaw baja;
//   mover ARRIBA        (dy < 0) -> mirar arriba        -> pitch sube.
//   pan con el botón central: la escena sigue al cursor, así que la CÁMARA se mueve al revés.
// -----------------------------------------------------------------------------
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace editor
{
	enum class Vista { Perspectiva, Superior, Frontal, Right };

	struct CameraOptions
	{
		float sensibilidad = 0.15f;          // grados por píxel
		bool invertirY = false;
		float velocidad = 6.0f;              // m/s con WASD
		float velocidadPan = 1.0f;           // 1 = la escena sigue al cursor píxel a píxel
		float velocidadZoom = 1.0f;
		float multiplicadorShift = 3.0f;
		float multiplicadorCtrl = 0.25f;
		float pasoCuadricula = 0.10f;        // m, para Ctrl+Shift + flechas
	};

	// Shift acelera, Ctrl da precisión. Con Ctrl y Shift a la vez PREVALECE CTRL (la precisión nunca debe acelerarse por accidente).
	inline float SpeedMultiplier(bool ctrl, bool shift, const CameraOptions& o)
	{
		if (ctrl) return o.multiplicadorCtrl;
		if (shift) return o.multiplicadorShift;
		return 1.0f;
	}

	constexpr float LimitePitch = 89.0f;   // grados; evita la inversión en los polos

	struct CamaraEditor
	{
		Vista vista = Vista::Perspectiva;
		glm::vec3 posicion{ 6.0f, -10.0f, 6.0f };
		float yaw = 96.0f;
		float pitch = -31.0f;
		float distanciaFoco = 12.0f;      // distancia al pivote de órbita/zoom (el punto que se mira)
		glm::vec3 centroOrto{ 0.0f };      // centro de las vistas ortográficas
		float escalaOrto = 14.0f;          // mitad de la altura visible en metros

		bool IsOrthographic() const { return vista != Vista::Perspectiva; }

		glm::vec3 Forward() const
		{
			const float y = glm::radians(yaw), p = glm::radians(pitch);
			return glm::vec3(std::cos(p) * std::cos(y), std::cos(p) * std::sin(y), std::sin(p));
		}
		glm::vec3 RightVector() const { const float y = glm::radians(yaw); return glm::vec3(std::sin(y), -std::cos(y), 0.0f); }
		glm::vec3 UpVector() const { return glm::cross(RightVector(), Forward()); }
		glm::vec3 PivotPoint() const { return IsOrthographic() ? centroOrto : posicion + Forward() * distanciaFoco; }

		void SetPitch(float grados) { pitch = std::clamp(grados, -LimitePitch, LimitePitch); }

		// Botón derecho: mirar alrededor (la cámara no se desplaza).
		void Look(float dx, float dy, const CameraOptions& o)
		{
			yaw -= dx * o.sensibilidad;
			SetPitch(pitch + (o.invertirY ? dy : -dy) * o.sensibilidad);
			yaw = std::remainder(yaw, 360.0f);
		}

		// Alt + botón izquierdo: la cámara orbita alrededor del pivote que tenía delante al empezar.
		void Orbit(const glm::vec3& pivote, float dx, float dy, const CameraOptions& o)
		{
			Look(dx, dy, o);
			posicion = pivote - Forward() * distanciaFoco;
		}

		// Botón central: pan. 'altoPx' es el alto del viewport; con velocidadPan = 1 la escena sigue al cursor.
		void Pan(float dx, float dy, float altoPx, const CameraOptions& o)
		{
			if (altoPx < 1.0f) return;
			if (IsOrthographic())
			{
				const float k = 2.0f * escalaOrto / altoPx * o.velocidadPan;
				const glm::mat3 base = OrthoBasis();
				centroOrto += (-base[0] * dx + base[1] * dy) * k;
				return;
			}
			const float k = 2.0f * distanciaFoco * std::tan(glm::radians(25.0f)) / altoPx * o.velocidadPan;
			posicion += (-RightVector() * dx + UpVector() * dy) * k;
		}

		// Rueda: positivo acerca. Perspectiva: dolly hacia el pivote; ortográfica: escala.
		void Zoom(float rueda, const CameraOptions& o)
		{
			const float f = std::exp(-rueda * 0.12f * o.velocidadZoom);
			if (IsOrthographic()) { escalaOrto = std::clamp(escalaOrto * f, 0.25f, 500.0f); return; }
			const glm::vec3 pivote = PivotPoint();
			distanciaFoco = std::clamp(distanciaFoco * f, 0.2f, 250.0f);
			posicion = pivote - Forward() * distanciaFoco;
		}

		// Botón derecho + WASD/QE (vuelo libre). adelante/derecha/subir en {-1, 0, 1}.
		void Move(float adelante, float derecha, float subir, float dt, float multiplicador, const CameraOptions& o)
		{
			const glm::vec3 d = Forward() * adelante + RightVector() * derecha + glm::vec3(0.0f, 0.0f, subir);
			posicion += d * (o.velocidad * multiplicador * std::clamp(dt, 0.0f, 0.05f));
		}

		void FocusOn(const glm::vec3& centro, float radio)
		{
			distanciaFoco = std::clamp(radio * 2.5f, 1.5f, 60.0f);
			posicion = centro - Forward() * distanciaFoco;
			centroOrto = centro;
			escalaOrto = std::clamp(radio * 1.6f, 1.0f, 100.0f);
		}

		// Ejes de pantalla (derecha, arriba, hacia la cámara) de las vistas ortográficas.
		glm::mat3 OrthoBasis() const
		{
			switch (vista)
			{
			case Vista::Frontal: return glm::mat3(glm::vec3(1, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, -1, 0));
			case Vista::Right: return glm::mat3(glm::vec3(0, 1, 0), glm::vec3(0, 0, 1), glm::vec3(1, 0, 0));
			default: return glm::mat3(glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1));
			}
		}

		void ChangeView(Vista nueva)
		{
			if (nueva == vista) return;
			if (!IsOrthographic()) centroOrto = PivotPoint();   // al entrar en una vista ortográfica se mira el punto que se miraba
			vista = nueva;
		}

		void Matrices(float aspecto, glm::mat4& v, glm::mat4& p) const
		{
			const float a = std::max(aspecto, 0.1f);
			if (!IsOrthographic())
			{
				v = glm::lookAt(posicion, posicion + Forward(), glm::vec3(0.0f, 0.0f, 1.0f));
				p = glm::perspective(glm::radians(50.0f), a, 0.05f, 1000.0f);
				return;
			}
			const glm::mat3 base = OrthoBasis();
			v = glm::lookAt(centroOrto + base[2] * 100.0f, centroOrto, base[1]);
			p = glm::ortho(-escalaOrto * a, escalaOrto * a, -escalaOrto, escalaOrto, 0.05f, 400.0f);
		}
	};

	// ------------------------------------------------------------------ Arbitraje de la entrada
	// La navegacion de camara y la seleccion por clic solo empiezan si ningun widget de ImGui, ningun gizmo ni ningun popup
	// tiene ya la entrada. (Prioridad: 1 widget de ImGui, 2 gizmo, 3 navegacion, 4 seleccion, 5 atajos.)
	inline bool FreeViewport(bool sobrePanel, bool imguiActivo, bool gizmoUsando, bool popup)
	{
		return !sobrePanel && !imguiActivo && !gizmoUsando && !popup;
	}

	// ImGuizmo queda habilitado salvo que la camara navegue, se pulse Alt o la escala de una puerta no se permita.
	// A PROPOSITO no depende de io.WantCaptureMouse: ImGuizmo lo activa el mismo al pasar sobre un manipulador.
	inline bool IsGizmoEnabled(bool usando, bool navegando, bool alt, bool escalaDePuerta)
	{
		return usando || (!navegando && !alt && !escalaDePuerta);
	}

	// Una selección por clic consume ese gesto completo. El gizmo solo puede empezar con una
	// pulsación nueva, después de que el botón izquierdo se haya soltado.
	struct GizmoPostSelectionLock
	{
		void Lock() { bloqueado = true; }
		bool AllowManipulation(bool botonIzquierdoPulsado)
		{
			if (!bloqueado) return true;
			if (!botonIzquierdoPulsado) bloqueado = false;
			return false;
		}
		void Reset() { bloqueado = false; }
		bool IsLocked() const { return bloqueado; }
	private:
		bool bloqueado = false;
	};

	inline void KeepHeightInTopView(glm::mat4& transformacion, const glm::mat4& original, bool vistaSuperior, bool herramientaMover)
	{
		if (vistaSuperior && herramientaMover) transformacion[3].z = original[3].z;
	}

	// ------------------------------------------------------------------ Mover objetos con las flechas
	enum class ArrowKey { Up, Abajo, Izquierda, Right, Subir, Bajar };

	// Longitud de un paso: 10 cm; Ctrl 1 cm; Shift 50 cm; Ctrl+Shift = "snap" a la cuadrícula (un paso de cuadrícula).
	inline float ArrowStepLength(bool ctrl, bool shift, const CameraOptions& o, bool& snap)
	{
		snap = ctrl && shift;
		if (snap) return o.pasoCuadricula;
		if (ctrl) return 0.01f;
		if (shift) return 0.50f;
		return 0.10f;
	}

	// Dirección unitaria en el MUNDO de una flecha. En perspectiva se proyecta sobre el plano horizontal (no cambia la altura);
	// en vistas ortográficas usa los ejes de pantalla (Top: derecha = +X, arriba = +Y).
	inline glm::vec3 ArrowDirection(ArrowKey f, const CamaraEditor& c)
	{
		if (f == ArrowKey::Subir) return glm::vec3(0, 0, 1);
		if (f == ArrowKey::Bajar) return glm::vec3(0, 0, -1);
		glm::vec3 derecha, arriba;
		if (c.IsOrthographic()) { const glm::mat3 b = c.OrthoBasis(); derecha = b[0]; arriba = b[1]; }
		else
		{
			derecha = glm::vec3(c.RightVector().x, c.RightVector().y, 0.0f);
			const glm::vec3 a = c.Forward();
			arriba = glm::vec3(a.x, a.y, 0.0f);
			const float l = glm::length(arriba);
			arriba = l > 1e-4f ? arriba / l : glm::vec3(derecha.y * -1.0f, derecha.x, 0.0f);   // mirando recto arriba/abajo: usa la derecha girada
		}
		switch (f)
		{
		case ArrowKey::Up: return arriba;
		case ArrowKey::Abajo: return -arriba;
		case ArrowKey::Right: return derecha;
		default: return -derecha;
		}
	}

	inline float SnapToGrid(float valor, float paso)
	{
		return paso > 1e-6f ? std::round(valor / paso) * paso : valor;
	}

	// Posición nueva tras una pulsación. Con snap, el resultado queda sobre la cuadrícula en los ejes que se mueven.
	inline glm::vec3 ApplyArrowKey(const glm::vec3& posicion, ArrowKey f, const CamaraEditor& c, bool ctrl, bool shift,
		const CameraOptions& o)
	{
		bool snap = false;
		const float paso = ArrowStepLength(ctrl, shift, o, snap);
		const glm::vec3 d = ArrowDirection(f, c);
		glm::vec3 nueva = posicion + d * paso;
		if (snap)
			for (int eje = 0; eje < 3; ++eje)
				if (std::abs(d[eje]) > 1e-3f) nueva[eje] = SnapToGrid(nueva[eje], o.pasoCuadricula);
		return nueva;
	}
}
