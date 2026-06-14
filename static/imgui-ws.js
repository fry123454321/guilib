var DOM_KEY_CODE_MIN = 1;
var DOM_KEY_CODE_MAX = 255;

var DOM_KEY_CODE = {
    TAB: 9,
    BACKSPACE: 8,
    ESCAPE: 27,
    SPACE: 32,
    PAGE_UP: 33,
    PAGE_DOWN: 34,
    END: 35,
    HOME: 36,
    LEFT: 37,
    UP: 38,
    RIGHT: 39,
    DOWN: 40,
    A: 65,
    V: 86,
};

var imgui_ws = {
    canvas: null,
    gl: null,

    shader_program: null,
    vertex_buffer: null,
    index_buffer: null,

    attribute_location_tex: null,
    attribute_location_proj_mtx: null,
    attribute_location_position: null,
    attribute_location_uv: null,
    attribute_location_color: null,

    device_pixel_ratio: window.devicePixelRatio || 1,
    ui_scale: 1.0,

    tex_map_id: {},
    tex_map_rev: {},
    tex_map_abuf: {},

    n_draw_lists: null,
    draw_lists_abuf: {},

    io: {
        mouse_x: 0.0,
        mouse_y: 0.0,

        want_capture_mouse: true,
        want_capture_keyboard: true,
    },

    // RGBA, диапазон 0..1
    clear_color: [0.45, 0.33, 0.60, 1.00],
    clear_color_rev: -1,
    clear_color_rev_pending: null,

    clipboard_rev: -1,
    clipboard_text_from_imgui: '',

    mod_ctrl: false,
    mod_meta: false,
    _modifiers_installed: false,
    _resize_installed: false,

    clipboard_log_max: 14,
    clipboard_log_lines: [],

    clipboard_quoted: function(text, maxLen) {
        text = text != null ? String(text) : '';
        if (maxLen && text.length > maxLen) {
            text = text.slice(0, maxLen) + '...';
        }
        return JSON.stringify(text);
    },

    clipboard_log: function(message) {
        console.log(message);
        this.clipboard_log_lines.unshift(message);
        if (this.clipboard_log_lines.length > this.clipboard_log_max) {
            this.clipboard_log_lines.length = this.clipboard_log_max;
        }
        var el = document.getElementById('clipboard-log');
        if (el) {
            el.textContent = this.clipboard_log_lines.join('\n');
        }
    },

    update_modifiers: function(event) {
        if (!event) {
            this.mod_ctrl = false;
            this.mod_meta = false;
            return;
        }
        var gms = event.getModifierState ? event.getModifierState.bind(event) : null;
        this.mod_ctrl = !!(event.ctrlKey || (gms && gms('Control')));
        this.mod_meta = !!(event.metaKey || (gms && gms('Meta')));
    },

    modifiers_active: function(event) {
        this.update_modifiers(event);
        return this.mod_ctrl || this.mod_meta;
    },

    is_paste_key: function(event) {
        return event.key === 'v' || event.key === 'V' ||
            event.code === 'KeyV' || event.keyCode === DOM_KEY_CODE.V;
    },

    is_select_all_key: function(event) {
        return this.modifiers_active(event) && (
            event.key === 'a' || event.key === 'A' ||
            event.code === 'KeyA' || event.keyCode === DOM_KEY_CODE.A);
    },

    is_text_input_key: function(event) {
        if (event.ctrlKey || event.metaKey || event.altKey) {
            return false;
        }
        if (event.key === 'Dead' || event.key === 'Process' || event.key === 'Unidentified') {
            return false;
        }
        if (event.key.length === 1) {
            var cp = event.key.charCodeAt(0);
            return cp >= DOM_KEY_CODE.SPACE || cp > 127;
        }
        return false;
    },

    // Tab and other keys Chrome uses for page focus / scrolling instead of sending to canvas.
    should_prevent_browser_default: function(event) {
        if (!this.io.want_capture_keyboard) {
            return false;
        }
        if (this.is_select_all_key(event)) {
            return true;
        }
        if (event.ctrlKey || event.metaKey || event.altKey) {
            return false;
        }
        var code = event.keyCode;
        if (code === DOM_KEY_CODE.TAB) {
            return true;
        }
        if (code === DOM_KEY_CODE.BACKSPACE ||
            code === DOM_KEY_CODE.ESCAPE ||
            code === DOM_KEY_CODE.SPACE ||
            (code >= DOM_KEY_CODE.PAGE_UP && code <= DOM_KEY_CODE.DOWN)) {
            return true;
        }
        return false;
    },

    send_text_input: function(incppect, text) {
        text = text != null ? String(text) : '';
        if (!text) {
            return;
        }
        var byte_len = new TextEncoder().encode(text).length;
        incppect.send('4 ' + byte_len + ' ' + text);
    },

    send_resize: function(incppect) {
        if (!this.canvas) {
            return;
        }

        var wrap = this.canvas.parentElement;
        if (wrap) {
            var dpr = window.devicePixelRatio || 1;
            var cssW = wrap.clientWidth;
            var cssH = wrap.clientHeight;
            if (cssW >= 1 && cssH >= 1) {
                this.canvas.width = Math.round(cssW * dpr);
                this.canvas.height = Math.round(cssH * dpr);
                this.canvas.style.width = cssW + 'px';
                this.canvas.style.height = cssH + 'px';
                this.device_pixel_ratio = dpr;
            }
        } else {
            this.device_pixel_ratio = window.devicePixelRatio || 1;
        }

        var dprSafe = this.device_pixel_ratio > 0.01 ? this.device_pixel_ratio : 1.0;
        var logicalW = this.canvas.width / dprSafe;
        var logicalH = this.canvas.height / dprSafe;
        if (logicalW < 256 || logicalH < 256) {
            return;
        }

        this.update_gl_display_size();
        incppect.send('7 ' + this.canvas.width + ' ' + this.canvas.height + ' ' + this.device_pixel_ratio);
    },

    schedule_send_resize: function(incppect) {
        var self = this;
        requestAnimationFrame(function() {
            requestAnimationFrame(function() {
                self.send_resize(incppect);
            });
        });
    },

    pointer_pos: function(event) {
        var rect = this.canvas.getBoundingClientRect();
        var scaleX = this.canvas.width / rect.width;
        var scaleY = this.canvas.height / rect.height;
        return {
            x: (event.clientX - rect.left) * scaleX,
            y: (event.clientY - rect.top) * scaleY,
        };
    },

    logical_size: function() {
        var scale = this.effective_ui_scale();
        return {
            w: this.canvas ? this.canvas.width / scale : 0,
            h: this.canvas ? this.canvas.height / scale : 0,
        };
    },

    effective_ui_scale: function() {
        var ui = this.ui_scale > 0.01 ? this.ui_scale : 1.0;
        var dpr = this.device_pixel_ratio > 0.01 ? this.device_pixel_ratio : 1.0;
        return ui * dpr;
    },

    incppect_ui_scale: function(incppect) {
        var scale = incppect.get_float('imgui.ui_scale');
        if (isFinite(scale) && scale > 0.01) {
            this.ui_scale = scale;
        }
    },

    update_gl_display_size: function() {
        if (!this.gl || !this.canvas || !this.shader_program) {
            return;
        }

        var physW = this.canvas.width;
        var physH = this.canvas.height;
        if (physW < 1 || physH < 1) {
            return;
        }

        var logical = this.logical_size();
        var w = logical.w;
        var h = logical.h;
        if (w < 1 || h < 1) {
            return;
        }

        this.gl.viewport(0, 0, physW, physH);

        var ortho_projection = new Float32Array([
            2.0 / w, 0.0, 0.0, 0.0,
            0.0, 2.0 / -h, 0.0, 0.0,
            0.0, 0.0, -1.0, 0.0,
            -1.0, 1.0, 0.0, 1.0,
        ]);
        this.gl.useProgram(this.shader_program);
        this.gl.uniformMatrix4fv(this.attribute_location_proj_mtx, false, ortho_projection);
    },

    install_modifier_tracking: function() {
        if (this._modifiers_installed) {
            return;
        }
        this._modifiers_installed = true;
        var self = this;
        var sync = function(e) { self.update_modifiers(e); };
        window.addEventListener('keydown', sync, true);
        window.addEventListener('keyup', sync, true);
        window.addEventListener('blur', function() { self.update_modifiers(null); });
    },

    set_clear_color: function(r, g, b, a) {
        if (arguments.length === 1 && Array.isArray(r)) {
            this.clear_color = r.slice(0, 4);
        } else {
            this.clear_color = [r, g, b, a !== undefined ? a : 1.0];
        }
        this.sync_page_background();
    },

    sync_page_background: function() {
        var c = this.clear_color;
        var css = 'rgb(' +
            Math.round(c[0] * 255) + ',' +
            Math.round(c[1] * 255) + ',' +
            Math.round(c[2] * 255) + ')';
        document.body.style.background = css;
        var wrap = document.getElementById('canvas-wrap');
        if (wrap) {
            wrap.style.background = css;
        }
    },

    clear_background: function() {
        var c = this.clear_color;
        this.gl.clearColor(c[0], c[1], c[2], c[3]);
        this.gl.clear(this.gl.COLOR_BUFFER_BIT);
    },

    init: function(canvas_name) {
        this.canvas = document.getElementById(canvas_name);
        this.install_modifier_tracking();

        var onkeyup = this.canvas_on_keyup.bind(this);
        var onkeydown = this.canvas_on_keydown.bind(this);
        var onkeypress = this.canvas_on_keypress.bind(this);
        var onpointermove = this.canvas_on_pointermove.bind(this);
        var onpointerdown = this.canvas_on_pointerdown.bind(this);
        var onpointerup = this.canvas_on_pointerup.bind(this);
        var onwheel = this.canvas_on_wheel.bind(this);
        var ontouch = this.canvas_on_touch.bind(this);

        //this.canvas.style.touchAction = 'none'; // Disable browser handling of all panning and zooming gestures.
        //this.canvas.addEventListener('blur', this.canvas_on_blur);

        this.canvas.addEventListener('keyup', onkeyup, true);
        this.canvas.addEventListener('keydown', onkeydown, true);
        this.canvas.addEventListener('keypress', onkeypress, true);

        this.canvas.addEventListener('pointermove', onpointermove);
        this.canvas.addEventListener('mousemove', onpointermove);

        this.canvas.addEventListener('pointerdown', onpointerdown);
        this.canvas.addEventListener('mousedown', onpointerdown);

        this.canvas.addEventListener('pointerup', onpointerup);
        this.canvas.addEventListener('mouseup', onpointerup);

        this.canvas.addEventListener('contextmenu', function(event) { event.preventDefault(); });
        this.canvas.addEventListener('wheel', onwheel, false);

        this.canvas.addEventListener('touchstart', ontouch);
        this.canvas.addEventListener('touchmove', ontouch);
        this.canvas.addEventListener('touchend', ontouch);
        this.canvas.addEventListener('touchcancel', ontouch);

        this.gl = this.canvas.getContext('webgl');

        this.vertex_buffer = this.gl.createBuffer();
        this.index_buffer = this.gl.createBuffer();

        var vertex_shader_source = [
            'precision mediump float;' +
            'uniform mat4 ProjMtx;' +
            'attribute vec2 Position;' +
            'attribute vec2 UV;' +
            'attribute vec4 Color;' +
            'varying vec2 Frag_UV;' +
            'varying vec4 Frag_Color;' +
            'void main(void) {' +
            '	Frag_UV = UV;' +
            '	Frag_Color = Color;' +
            '   gl_Position = ProjMtx * vec4(Position, 0, 1);' +
            '}'
        ];

        var vertex_shader = this.gl.createShader(this.gl.VERTEX_SHADER);
        this.gl.shaderSource(vertex_shader, vertex_shader_source);
        this.gl.compileShader(vertex_shader);

        var fragment_shader_source = [
            'precision mediump float;' +
            'uniform sampler2D Texture;' +
            'varying vec2 Frag_UV;' +
            'varying vec4 Frag_Color;' +
            'void main() {' +
            '	gl_FragColor = Frag_Color * texture2D(Texture, Frag_UV.st);' +
            '}'
        ];

        var fragment_shader = this.gl.createShader(this.gl.FRAGMENT_SHADER);
        this.gl.shaderSource(fragment_shader, fragment_shader_source);
        this.gl.compileShader(fragment_shader);

        this.shader_program = this.gl.createProgram();
        this.gl.attachShader(this.shader_program, vertex_shader);
        this.gl.attachShader(this.shader_program, fragment_shader);
        this.gl.linkProgram(this.shader_program);

        this.attribute_location_tex      = this.gl.getUniformLocation(this.shader_program,   "Texture");
        this.attribute_location_proj_mtx = this.gl.getUniformLocation(this.shader_program,   "ProjMtx");
        this.attribute_location_position = this.gl.getAttribLocation(this.shader_program,    "Position");
        this.attribute_location_uv       = this.gl.getAttribLocation(this.shader_program,    "UV");
        this.attribute_location_color    = this.gl.getAttribLocation(this.shader_program,    "Color");

        this.gl.enable(this.gl.BLEND);
        this.gl.blendEquation(this.gl.FUNC_ADD);
        this.gl.blendFunc(this.gl.SRC_ALPHA, this.gl.ONE_MINUS_SRC_ALPHA);
        this.gl.disable(this.gl.CULL_FACE);
        this.gl.disable(this.gl.DEPTH_TEST);

        this.gl.useProgram(this.shader_program);
        this.gl.uniform1i(this.attribute_location_tex, 0);
        this.update_gl_display_size();

        this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.vertex_buffer);
        this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, this.index_buffer);

        this.gl.enableVertexAttribArray(this.attribute_location_position);
        this.gl.enableVertexAttribArray(this.attribute_location_uv);
        this.gl.enableVertexAttribArray(this.attribute_location_color);
        this.gl.vertexAttribPointer(this.attribute_location_position, 2, this.gl.FLOAT,         false, 5*4, 0);
        this.gl.vertexAttribPointer(this.attribute_location_uv,       2, this.gl.FLOAT,         false, 5*4, 2*4);
        this.gl.vertexAttribPointer(this.attribute_location_color,    4, this.gl.UNSIGNED_BYTE, true,  5*4, 4*4);
        this.sync_page_background();
    },

    incppect_textures: function(incppect) {
        var n_textures = incppect.get_int32('imgui.n_textures');

        for (var i = 0; i < n_textures; ++i) {
            var tex_id = incppect.get_int32('imgui.texture_id[%d]', i);
            var tex_rev = incppect.get_int32('imgui.texture_revision[%d]', tex_id);

            if (this.tex_map_abuf[tex_id] == null || this.tex_map_abuf[tex_id].byteLength < 1) {
                this.tex_map_abuf[tex_id] = incppect.get_abuf('imgui.texture_data[%d]', tex_id);
            } else if (this.tex_map_abuf[tex_id] && (this.tex_map_id[tex_id] == null || this.tex_map_rev[tex_id] != tex_rev)) {
                this.tex_map_abuf[tex_id] = incppect.get_abuf('imgui.texture_data[%d]', tex_id);
                imgui_ws.init_tex(tex_id, tex_rev, this.tex_map_abuf[tex_id]);
            }
        }
    },

    init_tex: function(tex_id, tex_rev, tex_abuf) {
        var tex_abuf_uint8 = new Uint8Array(tex_abuf);
        var tex_abuf_int32 = new Int32Array(tex_abuf);

        const type = tex_abuf_int32[1];
        const width = tex_abuf_int32[2];
        const height = tex_abuf_int32[3];
        const revision = tex_abuf_int32[4];

        if (this.tex_map_rev[tex_id] && revision == this.tex_map_rev[tex_id]) {
            return;
        }

        var pixels = new Uint8Array(4*width*height);

        if (type == 0) { // Alpha8
            for (var i = 0; i < width*height; ++i) {
                pixels[4*i + 0] = 0xFF;
                pixels[4*i + 1] = 0xFF;
                pixels[4*i + 2] = 0xFF;
                pixels[4*i + 3] = tex_abuf_uint8[20 + i];
            }
        } else if (type == 1) { // Gray8
            for (var i = 0; i < width*height; ++i) {
                pixels[4*i + 0] = tex_abuf_uint8[20 + i];
                pixels[4*i + 1] = tex_abuf_uint8[20 + i];
                pixels[4*i + 2] = tex_abuf_uint8[20 + i];
                pixels[4*i + 3] = 0xFF;
            }
        } else if (type == 2) { // RGB24
            for (var i = 0; i < width*height; ++i) {
                pixels[4*i + 0] = tex_abuf_uint8[20 + 3*i + 0];
                pixels[4*i + 1] = tex_abuf_uint8[20 + 3*i + 1];
                pixels[4*i + 2] = tex_abuf_uint8[20 + 3*i + 2];
                pixels[4*i + 3] = 0xFF;
            }
        } else if (type == 3) { // RGBA32
            for (var i = 0; i < width*height; ++i) {
                pixels[4*i + 0] = tex_abuf_uint8[20 + 4*i + 0];
                pixels[4*i + 1] = tex_abuf_uint8[20 + 4*i + 1];
                pixels[4*i + 2] = tex_abuf_uint8[20 + 4*i + 2];
                pixels[4*i + 3] = tex_abuf_uint8[20 + 4*i + 3];
            }
        }

        this.tex_map_rev[tex_id] = tex_rev;

        if (this.tex_map_id[tex_id]) {
            this.gl.bindTexture(this.gl.TEXTURE_2D, this.tex_map_id[tex_id]);
            this.gl.texImage2D(this.gl.TEXTURE_2D, 0, this.gl.RGBA, width, height, 0, this.gl.RGBA, this.gl.UNSIGNED_BYTE, pixels);
        } else {
            this.tex_map_id[tex_id] = this.gl.createTexture();
            this.gl.bindTexture(this.gl.TEXTURE_2D, this.tex_map_id[tex_id]);
            this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MIN_FILTER, this.gl.LINEAR);
            this.gl.texParameteri(this.gl.TEXTURE_2D, this.gl.TEXTURE_MAG_FILTER, this.gl.LINEAR);
            //this.gl.pixelStorei(gl.UNPACK_ROW_LENGTH); // WebGL2
            this.gl.texImage2D(this.gl.TEXTURE_2D, 0, this.gl.RGBA, width, height, 0, this.gl.RGBA, this.gl.UNSIGNED_BYTE, pixels);
        }
    },

    incppect_draw_lists: function(incppect) {
        this.n_draw_lists = incppect.get_int32('imgui.n_draw_lists');
        if (this.n_draw_lists < 1) return;

        for (var i = 0; i < this.n_draw_lists; ++i) {
            this.draw_lists_abuf[i] = incppect.get_abuf('imgui.draw_list[%d]', i);
        }
    },

    send_paste_to_server: function(incppect, text) {
        text = text != null ? String(text) : '';
        var byte_len = new TextEncoder().encode(text).length;
        incppect.send('9 ' + byte_len + ' ' + text);
    },

    decode_clipboard_abuf: function(abuf) {
        if (!abuf || !abuf.byteLength) {
            return '';
        }
        var bytes = new Uint8Array(abuf);
        var nul = bytes.indexOf(0);
        if (nul >= 0) {
            bytes = bytes.subarray(0, nul);
        }
        return new TextDecoder('utf-8').decode(bytes);
    },

    clipboard_fallback_paste_to_imgui: function(incppect) {
        var el = document.getElementById('clipboard-fallback-text');
        var text = el ? el.value : '';
        this.clipboard_log('костыль: вставка из поля в ImGui ' + this.clipboard_quoted(text));
        this.send_paste_to_server(incppect, text);
    },

    clipboard_fallback_pull_from_imgui: function() {
        var el = document.getElementById('clipboard-fallback-text');
        var text = this.clipboard_text_from_imgui != null ? String(this.clipboard_text_from_imgui) : '';
        if (el) {
            el.value = text;
            el.focus();
            el.select();
        }
        this.clipboard_log('костыль: из ImGui в поле ' + this.clipboard_quoted(text));
    },

    paste_from_browser: function(incppect) {
        var self = this;

        if (!navigator.clipboard || !navigator.clipboard.readText) {
            var empty = '';
            self.clipboard_log('попытка вставить текст: ' + self.clipboard_quoted(empty));
            self.send_paste_to_server(incppect, empty);
            return;
        }

        navigator.clipboard.readText().then(function(text) {
            text = text != null ? String(text) : '';
            self.clipboard_log('попытка вставить текст: ' + self.clipboard_quoted(text));
            self.send_paste_to_server(incppect, text);
        }).catch(function() {
            var empty = '';
            self.clipboard_log('попытка вставить текст: ' + self.clipboard_quoted(empty));
            self.send_paste_to_server(incppect, empty);
        });
    },

    incppect_clipboard: function(incppect) {
        // rev и text всегда в одном poll — иначе rev обновляется на кадр раньше текста
        incppect.get_int32('imgui.clipboard_rev');
        incppect.get_abuf('imgui.clipboard_text');

        var abuf = incppect.get_abuf('imgui.clipboard_text');
        var text = this.decode_clipboard_abuf(abuf);
        this.clipboard_text_from_imgui = text;

        var rev = incppect.get_int32('imgui.clipboard_rev');
        if (!isFinite(rev) || rev === this.clipboard_rev) {
            return;
        }

        // после перезагрузки страницы не считаем старый rev на сервере новым копированием
        if (this.clipboard_rev < 0) {
            this.clipboard_rev = rev;
            return;
        }

        this.clipboard_rev = rev;

        this.clipboard_log('попытка скопировать текст ' + this.clipboard_quoted(text));

        if (navigator.clipboard && navigator.clipboard.writeText) {
            navigator.clipboard.writeText(text).catch(function() {});
        }
    },

    incppect_clear_color: function(incppect) {
        // rev и компоненты в одном poll — как у clipboard
        incppect.get_int32('imgui.clear_color_rev');
        incppect.get_float('imgui.clear_color[%d]', 0);
        incppect.get_float('imgui.clear_color[%d]', 1);
        incppect.get_float('imgui.clear_color[%d]', 2);
        incppect.get_float('imgui.clear_color[%d]', 3);

        var rev = incppect.get_int32('imgui.clear_color_rev');
        if (!isFinite(rev) || rev === this.clear_color_rev) {
            return;
        }

        var r = incppect.get_float('imgui.clear_color[%d]', 0);
        var g = incppect.get_float('imgui.clear_color[%d]', 1);
        var b = incppect.get_float('imgui.clear_color[%d]', 2);
        var a = incppect.get_float('imgui.clear_color[%d]', 3);
        if (!isFinite(r) || !isFinite(g) || !isFinite(b)) {
            return;
        }

        this.clear_color_rev = rev;
        this.clear_color_rev_pending = null;
        this.set_clear_color(r, g, b, isFinite(a) ? a : 1.0);
    },

    render: function(n_draw_lists, draw_lists_abuf) {
        if (typeof n_draw_lists === "undefined" && typeof draw_lists_abuf === "undefined") {
            if (this.n_draw_lists === null) return;
            if (this.draw_lists_abuf === null) return;

            this.render(this.n_draw_lists, this.draw_lists_abuf);

            return;
        }

        this.update_gl_display_size();

        var uiScale = this.effective_ui_scale();
        var logical = this.logical_size();
        var clip_off_x = 0.0;
        var clip_off_y = 0.0;

        this.gl.enable(this.gl.SCISSOR_TEST);

        for (var i_list = 0; i_list < n_draw_lists; ++i_list) {
            if (draw_lists_abuf[i_list].byteLength < 1) continue;

            var draw_data_offset = 0;

            var p = new Float32Array(draw_lists_abuf[i_list], draw_data_offset, 2);
            var offset_x = p[0]; draw_data_offset += 4;
            var offset_y = p[1]; draw_data_offset += 4;

            p = new Uint32Array(draw_lists_abuf[i_list], draw_data_offset, 1);
            var n_vertices = p[0]; draw_data_offset += 4;

            var av = new Float32Array(draw_lists_abuf[i_list], draw_data_offset, 5*n_vertices);

            for (var k = 0; k < n_vertices; ++k) {
                av[5*k + 0] += offset_x;
                av[5*k + 1] += offset_y;
            }

            this.gl.bindBuffer(this.gl.ARRAY_BUFFER, this.vertex_buffer);
            this.gl.bufferData(this.gl.ARRAY_BUFFER, av, this.gl.STREAM_DRAW);

            for (var k = 0; k < n_vertices; ++k) {
                av[5*k + 0] -= offset_x;
                av[5*k + 1] -= offset_y;
            }

            draw_data_offset += 5*4*n_vertices;

            p = new Uint32Array(draw_lists_abuf[i_list], draw_data_offset, 1);
            var n_indices = p[0]; draw_data_offset += 4;

            var ai = new Uint16Array(draw_lists_abuf[i_list], draw_data_offset, n_indices);
            this.gl.bindBuffer(this.gl.ELEMENT_ARRAY_BUFFER, this.index_buffer);
            this.gl.bufferData(this.gl.ELEMENT_ARRAY_BUFFER, ai, this.gl.STREAM_DRAW);
            draw_data_offset += 2*n_indices;

            p = new Uint32Array(draw_lists_abuf[i_list], draw_data_offset, 1);
            var n_cmd = p[0]; draw_data_offset += 4;

            for (var i_cmd = 0; i_cmd < n_cmd; ++i_cmd) {
                var pi = new Uint32Array(draw_lists_abuf[i_list], draw_data_offset, 4);
                var n_elements = pi[0]; draw_data_offset += 4;
                var texture_id = pi[1]; draw_data_offset += 4;
                var offset_vtx = pi[2]; draw_data_offset += 4;
                var offset_idx = pi[3]; draw_data_offset += 4;

                var pf = new Float32Array(draw_lists_abuf[i_list], draw_data_offset, 4);
                var clip_x = pf[0] - clip_off_x; draw_data_offset += 4;
                var clip_y = pf[1] - clip_off_y; draw_data_offset += 4;
                var clip_z = pf[2] - clip_off_x; draw_data_offset += 4;
                var clip_w = pf[3] - clip_off_y; draw_data_offset += 4;

                if (clip_x < logical.w && clip_y < logical.h && clip_z >= 0.0 && clip_w >= 0.0) {
                    var sx = clip_x * uiScale;
                    var sy = clip_y * uiScale;
                    var sz = clip_z * uiScale;
                    var sw = clip_w * uiScale;
                    this.gl.scissor(sx, this.canvas.height - sw, sz - sx, sw - sy);
                    if (texture_id in this.tex_map_id) {
                        this.gl.activeTexture(this.gl.TEXTURE0);
                        this.gl.bindTexture(this.gl.TEXTURE_2D, this.tex_map_id[texture_id]);
                    } else {
                        this.gl.bindTexture(this.gl.TEXTURE_2D, null);
                    }
                    this.gl.drawElements(this.gl.TRIANGLES, n_elements, this.gl.UNSIGNED_SHORT, 2*offset_idx);
                }
            }
        }

        this.gl.disable(this.gl.SCISSOR_TEST);
    },

    canvas_on_keyup: function(event) {},
    canvas_on_keydown: function(event) {},
    canvas_on_keypress: function(event) {},
    canvas_on_pointermove: function(event) {},
    canvas_on_pointerdown: function(event) {},
    canvas_on_pointerup: function(event) {},
    canvas_on_wheel: function(event) {},
    canvas_on_touch: function(event) {},

    set_incppect_handlers: function(incppect) {
        var self = this;
        this.schedule_send_resize(incppect);
        if (!this._resize_installed) {
            this._resize_installed = true;
            window.addEventListener('resize', function() {
                self.schedule_send_resize(incppect);
            });
        }

        this.canvas_on_keyup = function(event) {
            var code = event.keyCode;
            if (code >= DOM_KEY_CODE_MIN && code <= DOM_KEY_CODE_MAX) {
                incppect.send('6 ' + code);
            }
            if (this.should_prevent_browser_default(event)) {
                event.preventDefault();
            }
        };

        this.canvas_on_keydown = function(event) {
            if (this.modifiers_active(event) && this.is_paste_key(event) && this.io.want_capture_keyboard) {
                event.preventDefault();
                this.paste_from_browser(incppect);
                return;
            }

            if (this.io.want_capture_keyboard && this.is_text_input_key(event)) {
                event.preventDefault();
                this.send_text_input(incppect, event.key);
                return;
            }

            var code = event.keyCode;
            if (code >= DOM_KEY_CODE_MIN && code <= DOM_KEY_CODE_MAX) {
                incppect.send('5 ' + code);
            }
            if (this.should_prevent_browser_default(event)) {
                event.preventDefault();
            }
        };

        this.canvas_on_keypress = function(event) {
            if (!this.io.want_capture_keyboard) {
                return;
            }
            if (this.is_select_all_key(event) ||
                (!event.ctrlKey && !event.metaKey && !event.altKey)) {
                event.preventDefault();
            }
        };

        this.canvas_on_pointermove = function(event) {
            var pos = this.pointer_pos(event);
            this.io.mouse_x = pos.x;
            this.io.mouse_y = pos.y;

            incppect.send('0 ' + this.io.mouse_x + ' ' + this.io.mouse_y);

            if (this.io.want_capture_mouse) {
                event.preventDefault();
            }
        };

        this.canvas_on_pointerdown = function(event) {
            if (this.canvas && typeof this.canvas.focus === 'function') {
                this.canvas.focus();
            }

            var pos = this.pointer_pos(event);
            this.io.mouse_x = pos.x;
            this.io.mouse_y = pos.y;

            incppect.send('1 ' + event.button + ' ' + this.io.mouse_x + ' ' + this.io.mouse_y);
        };

        this.canvas_on_pointerup = function(event) {
            incppect.send('2 ' + event.button + ' ' + this.io.mouse_x + ' ' + this.io.mouse_y);

            if (this.io.want_capture_mouse) {
                event.preventDefault();
            }
        };

        this.canvas_on_wheel = function(event) {
            let scale = 1.0;
            switch (event.deltaMode) {
                case event.DOM_DELTA_PIXEL:
                    scale = 0.01;
                    break;
                case event.DOM_DELTA_LINE:
                    scale = 0.2;
                    break;
                case event.DOM_DELTA_PAGE:
                    scale = 1.0;
                    break;
            }

            var wheel_x =  event.deltaX * scale;
            var wheel_y = -event.deltaY * scale;

            incppect.send('3 ' + wheel_x + ' ' + wheel_y);

            if (this.io.want_capture_mouse) {
                event.preventDefault();
            }
        };

        this.canvas_on_touch = function (event) {
            if (event.touches.length > 1) {
                return;
            }

            var touches = event.changedTouches,
                first = touches[0],
                type = "";

            switch(event.type) {
                case "touchstart": type = "mousedown"; break;
                case "touchmove":  type = "mousemove"; break;
                case "touchend":   type = "mouseup";   break;
                default:           return;
            }

            // initMouseEvent(type, canBubble, cancelable, view, clickCount,
            //                screenX, screenY, clientX, clientY, ctrlKey,
            //                altKey, shiftKey, metaKey, button, relatedTarget);

            var simulatedEvent = document.createEvent("MouseEvent");
            simulatedEvent.initMouseEvent(type, true, true, window, 1, first.screenX, first.screenY, first.clientX, first.clientY, false, false, false, false, 0/*left*/, null);

            first.target.dispatchEvent(simulatedEvent);
            event.preventDefault();
        };
    },

}
