extends Node3D
## A prova do motor: o Godot 4 no Switch homebrew desenha 3D com sombra, toca duas faixas sincronizadas
## (como um jogo de ritmo) e lê Joy-Con e toque. B cala e devolve a faixa da batida; o resto aparece na
## tela. O som é gerado aqui mesmo (nenhum arquivo de áudio), para o exemplo não depender de gravação.

const TAXA := 22050
const BPM := 120.0
const COMPASSOS := 2

var cubo: MeshInstance3D
var texto: Label
var musica: AudioStreamPlayer
var batida_ligada := true
var eventos: Array[String] = []
var toques := {}


func _ready() -> void:
	var camera := Camera3D.new()
	camera.position = Vector3(0, 1.2, 3.2)
	add_child(camera)
	camera.look_at(Vector3.ZERO)
	var luz := DirectionalLight3D.new()
	luz.rotation_degrees = Vector3(-45, 35, 0)
	luz.shadow_enabled = true
	add_child(luz)
	var ambiente := WorldEnvironment.new()
	ambiente.environment = Environment.new()
	ambiente.environment.background_mode = Environment.BG_COLOR
	ambiente.environment.background_color = Color("#1b2333")
	ambiente.environment.ambient_light_color = Color("#607080")
	add_child(ambiente)
	cubo = MeshInstance3D.new()
	cubo.mesh = BoxMesh.new()
	var material := StandardMaterial3D.new()
	material.albedo_color = Color("#478cbf")
	cubo.material_override = material
	add_child(cubo)
	var chao := MeshInstance3D.new()
	chao.mesh = PlaneMesh.new()
	(chao.mesh as PlaneMesh).size = Vector2(6, 6)
	chao.position.y = -0.8
	add_child(chao)

	var camada := CanvasLayer.new()
	add_child(camada)
	texto = Label.new()
	texto.position = Vector2(24, 20)
	texto.add_theme_font_size_override("font_size", 22)
	camada.add_child(texto)

	# duas faixas tocando juntas, como as de um jogo de ritmo (a base e a batida)
	var faixas := AudioStreamSynchronized.new()
	faixas.stream_count = 2
	faixas.set_sync_stream(0, _faixa(_base))
	faixas.set_sync_stream(1, _faixa(_batida))
	musica = AudioStreamPlayer.new()
	musica.stream = faixas
	add_child(musica)
	musica.play()


## Uma faixa em laço de COMPASSOS compassos (4/4), com `amostra(t)` dando o valor em -1..1 no tempo t (s).
func _faixa(amostra: Callable) -> AudioStreamWAV:
	var n := int(TAXA * 60.0 / BPM * 4 * COMPASSOS)
	var dados := PackedByteArray()
	dados.resize(n * 2)
	for i in n:
		dados.encode_s16(i * 2, int(clampf(amostra.call(float(i) / TAXA), -1.0, 1.0) * 26000.0))
	var wav := AudioStreamWAV.new()
	wav.format = AudioStreamWAV.FORMAT_16_BITS
	wav.mix_rate = TAXA
	wav.data = dados
	wav.loop_mode = AudioStreamWAV.LOOP_FORWARD
	wav.loop_end = n
	return wav


## A base: uma nota grave por tempo, subindo o arpejo (lá, dó, mi, sol).
func _base(t: float) -> float:
	var tempo := t * BPM / 60.0
	var nota: float = [110.0, 130.81, 164.81, 196.0][int(tempo) % 4]
	var dentro := fmod(tempo, 1.0)
	return sin(TAU * nota * t) * 0.5 * exp(-dentro * 3.0)


## A batida: um bumbo (seno que desce) em cada tempo e um estalo no contratempo.
func _batida(t: float) -> float:
	var tempo := t * BPM / 60.0
	var dentro := fmod(tempo, 1.0) * 60.0 / BPM
	var bumbo := sin(TAU * (50.0 + 90.0 * exp(-dentro * 30.0)) * dentro) * exp(-dentro * 12.0)
	var meio := fmod(tempo + 0.5, 1.0) * 60.0 / BPM
	var estalo := (randf() * 2.0 - 1.0) * exp(-meio * 60.0) * 0.5
	return bumbo + estalo


func _process(delta: float) -> void:
	cubo.rotate_y(delta * 1.2)
	cubo.rotate_x(delta * 0.5)
	var linhas := PackedStringArray()
	linhas.append("Godot %s no %s · %s" % [Engine.get_version_info().string, OS.get_name(), RenderingServer.get_video_adapter_name()])
	if Engine.has_singleton("Switch"):
		var switch: Object = Engine.get_singleton("Switch")
		linhas.append("jogo %s · motor %s · vídeo %s · %s%s" % [switch.versao(), switch.versao_do_motor(), switch.renderizador(), switch.modelo(), " na base" if switch.na_base() else ""])
	linhas.append("%d FPS · tela %s · música %.1f s" % [Engine.get_frames_per_second(), DisplayServer.window_get_size(), musica.get_playback_position()])
	linhas.append("batida %s (B cala e devolve)" % ("ligada" if batida_ligada else "calada"))
	for c in Input.get_connected_joypads():
		var eixos := "LX %.2f LY %.2f RX %.2f RY %.2f ZL %.0f ZR %.0f" % [Input.get_joy_axis(c, JOY_AXIS_LEFT_X), Input.get_joy_axis(c, JOY_AXIS_LEFT_Y), Input.get_joy_axis(c, JOY_AXIS_RIGHT_X), Input.get_joy_axis(c, JOY_AXIS_RIGHT_Y), Input.get_joy_axis(c, JOY_AXIS_TRIGGER_LEFT), Input.get_joy_axis(c, JOY_AXIS_TRIGGER_RIGHT)]
		linhas.append("controle %d: %s · %s" % [c, Input.get_joy_name(c), eixos])
	linhas.append("toques: %s" % str(toques.values()))
	linhas.append("últimos: " + ", ".join(eventos))
	texto.text = "\n".join(linhas)


func _input(evento: InputEvent) -> void:
	if evento is InputEventJoypadButton and evento.pressed:
		eventos.append("j%d botão %d" % [evento.device, evento.button_index])
		if evento.button_index == JOY_BUTTON_A:  # o B da Nintendo (o de baixo)
			batida_ligada = not batida_ligada
			(musica.stream as AudioStreamSynchronized).set_sync_stream_volume(1, 0.0 if batida_ligada else -80.0)
	elif evento is InputEventScreenTouch:
		if evento.pressed:
			toques[evento.index] = Vector2i(evento.position)
		else:
			toques.erase(evento.index)
		eventos.append("toque %d %s" % [evento.index, "desceu" if evento.pressed else "subiu"])
	elif evento is InputEventScreenDrag:
		toques[evento.index] = Vector2i(evento.position)
	if eventos.size() > 6:
		eventos = eventos.slice(eventos.size() - 6)
