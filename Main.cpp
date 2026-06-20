// ============================================================
//  Processamento Grafico - GRAU B
//  Aluno: Enzo Gabriel Franco Lanius e Felipe Plentz Klein
//
//  Dependencias: GLFW3, GLAD (glad.c + glad/glad.h)
// ============================================================

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <fstream>
#include <sstream>

// ============================================================
//  Dimensoes e constantes
// ============================================================
#define TILE_W   96
#define TILE_H   48
#define COLUNAS  18
#define LINHAS   18
#define WIN_W   1024
#define WIN_H    680
#define HUD_H     60

// ============================================================
//  Tipos de tile
// ============================================================
#define T_PAREDE   0   // bloqueia
#define T_CHAO     1   // caminhavel basico
#define T_AGUA     2   // bloqueia (nao pode entrar)
#define T_AREIA    3   // caminhavel
#define T_GRAMA    4   // caminhavel
#define T_LAVA     5   // caminhavel mas mata o jogador
#define T_GELO     6   // caminhavel
#define T_PILAR    7   // bloqueia
#define T_SAIDA    8   // objetivo final

// Walkable definido por tipo (1=sim, 0=nao)
// Tambem lido do arquivo de configuracao
int walkable[9] = { 0, 1, 0, 1, 1, 1, 1, 0, 1 };

// ============================================================
//  Mapa padrao 18x18 (usado se map.txt nao for encontrado)
// ============================================================
int mapa[LINHAS][COLUNAS] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,1,1,1,1,2,2,1,1,1,1,1,6,6,6,1,1,0},
    {0,1,7,1,1,2,2,1,4,4,4,1,6,7,6,1,1,0},
    {0,1,1,3,3,1,1,4,4,4,4,1,6,6,6,3,1,0},
    {0,1,1,3,7,1,1,4,7,4,4,1,1,1,3,3,1,0},
    {0,1,5,5,1,1,1,4,4,4,1,1,1,3,3,7,1,0},
    {0,1,5,5,5,1,1,1,1,1,1,1,1,1,3,3,1,0},
    {0,1,5,7,5,1,0,0,1,1,1,0,0,1,1,1,1,0},
    {0,1,1,1,1,1,0,0,1,2,1,0,0,1,6,6,1,0},
    {0,0,1,1,1,0,0,1,1,2,1,1,0,1,6,7,1,0},
    {0,0,0,1,1,0,1,1,1,1,1,1,1,1,6,6,1,0},
    {0,1,1,1,1,1,1,7,1,1,1,7,1,1,1,1,1,0},
    {0,1,3,3,1,5,5,1,1,1,1,1,5,5,1,4,4,0},
    {0,1,3,1,1,5,1,1,1,1,1,1,1,5,1,4,1,0},
    {0,1,1,1,1,1,1,1,2,2,2,1,1,1,1,1,1,0},
    {0,1,6,6,1,1,1,1,2,8,2,1,1,1,6,6,1,0},
    {0,1,6,1,1,1,1,1,2,2,2,1,1,1,1,6,1,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};

// ============================================================
//  Itens no mapa (moedas)
// ============================================================
#define MAX_ITENS 20
struct Item {
    int col, lin;
    int tipo;    // 0=moeda
    int ativo;
};
Item itens[MAX_ITENS];
int  num_itens = 0;

// Moedas padrao (usadas se map.txt nao tiver secao ITEMS)
void itensDefault() {
    num_itens = 0;
    int pos[][2] = {
        {3,2},{8,2},{13,2},{3,12},{13,12},{9,15},
        {2,5},{14,5},{5,13},{11,13}
    };
    for (int i = 0; i < 10; i++) {
        itens[i] = { pos[i][0], pos[i][1], 0, 1 };
        num_itens++;
    }
}

// ============================================================
//  Jogador
// ============================================================
int   jog_col = 1;
int   jog_lin = 1;
int   jog_dir = 0;       // 0=E,1=SE,2=S,3=SW,4=W,5=NW,6=N,7=NE
int   jog_vidas = 3;
int   jog_moedas = 0;
int   jog_invincivel = 0; // frames de invencibilidade apos dano
float anim_pernas = 0;    // oscilacao das pernas (sprite animado)
int   jog_andando = 0;

// ============================================================
//  Estado do jogo
// ============================================================
#define ESTADO_JOGANDO   0
#define ESTADO_VITORIA   1
#define ESTADO_GAME_OVER 2
int estado_jogo = ESTADO_JOGANDO;

char mensagem[80] = "Colete todas as moedas e chegue a SAIDA!";
int  msg_timer = 180;

float tempo = 0.0f;

// ============================================================
//  OpenGL infra (shaders + VAO/VBO)
// ============================================================
GLuint shaderProgram;
GLuint VAO, VBO;
GLint  uColor, uAlpha, uProjection;
float  projMatrix[16];

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char b[512]; glGetShaderInfoLog(s, 512, NULL, b); fprintf(stderr, "%s\n", b); }
    return s;
}

static void buildShaders() {
    const char* vs = R"(
        #version 330 core
        layout(location=0) in vec2 aPos;
        uniform mat4 uProjection;
        void main(){ gl_Position = uProjection * vec4(aPos,0.0,1.0); }
    )";
    const char* fs = R"(
        #version 330 core
        uniform vec3 uColor; uniform float uAlpha;
        out vec4 FragColor;
        void main(){ FragColor = vec4(uColor, uAlpha); }
    )";
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, v); glAttachShader(shaderProgram, f);
    glLinkProgram(shaderProgram);
    glDeleteShader(v); glDeleteShader(f);
    uColor = glGetUniformLocation(shaderProgram, "uColor");
    uAlpha = glGetUniformLocation(shaderProgram, "uAlpha");
    uProjection = glGetUniformLocation(shaderProgram, "uProjection");
}

static void setupVAO() {
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
    glBindVertexArray(VAO); glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, 256 * 2 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

static void ortho2D(float* m, float l, float r, float b, float t) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = 2.f / (r - l); m[5] = 2.f / (t - b); m[10] = -1.f;
    m[12] = -(r + l) / (r - l); m[13] = -(t + b) / (t - b); m[15] = 1.f;
}

// ============================================================
//  Primitivas de desenho
// ============================================================
static void prim(GLenum mode, float* v, int n,
    float r, float g, float b, float a = 1.f) {
    glUseProgram(shaderProgram);
    glUniform3f(uColor, r, g, b); glUniform1f(uAlpha, a);
    glUniformMatrix4fv(uProjection, 1, GL_FALSE, projMatrix);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, n * 2 * sizeof(float), v);
    glDrawArrays(mode, 0, n);
    glBindVertexArray(0);
}

static void quad(float x, float y, float w, float h,
    float r, float g, float b, float a = 1.f) {
    float v[] = { x,y, x + w,y, x + w,y + h, x,y, x + w,y + h, x,y + h };
    prim(GL_TRIANGLES, v, 6, r, g, b, a);
}

static void circle(float cx, float cy, float rad,
    float r, float g, float b, float a = 1.f) {
    const int N = 24;
    float v[2 + (N + 1) * 2];
    v[0] = cx; v[1] = cy;
    for (int i = 0;i <= N;i++) {
        float ang = i * 2.f * 3.14159f / N;
        v[2 + i * 2] = cx + cosf(ang) * rad; v[3 + i * 2] = cy + sinf(ang) * rad;
    }
    prim(GL_TRIANGLE_FAN, v, 2 + N + 1, r, g, b, a);
}

static void tri(float x1, float y1, float x2, float y2, float x3, float y3,
    float r, float g, float b, float a = 1.f) {
    float v[] = { x1,y1,x2,y2,x3,y3 };
    prim(GL_TRIANGLES, v, 3, r, g, b, a);
}

static void lineLoop(float* v, int n, float r, float g, float b, float a = 1.f) {
    prim(GL_LINE_LOOP, v, n, r, g, b, a);
}

// ============================================================
//  Projecao Isometrica Diamond
// ============================================================
#define ISO_OX  (WIN_W / 2.0f)
#define ISO_OY  (HUD_H + (WIN_H - HUD_H) * 0.82f)

static float isoX(int col, int lin) {
    return ISO_OX + (col - lin) * (TILE_W / 2.f);
}
static float isoY(int col, int lin) {
    return ISO_OY - (col + lin) * (TILE_H / 2.f);
}

// ============================================================
//  Tile isometrico com face 3D
// ============================================================
static void isoTile(float lx, float ly,
    float r, float g, float b,
    float rs, float gs, float bs,
    float a = 1.f) {
    float hw = TILE_W / 2.f, hh = TILE_H / 2.f;
    float tx = lx + hw, ty = ly + TILE_H;
    float rx = lx + TILE_W, ry = ly + hh;
    float bx = lx + hw, by = ly;
    float llx = lx, lly = ly + hh;

    // face superior
    float fs[] = { tx,ty,rx,ry,bx,by, tx,ty,bx,by,llx,lly };
    prim(GL_TRIANGLES, fs, 6, r, g, b, a);
    float fb[] = { tx,ty,rx,ry,bx,by,llx,lly };
    lineLoop(fb, 4, r * .5f, g * .5f, b * .5f, a);

    // profundidade
    float depth = hh * .45f;
    float fl[] = { llx,lly,bx,by,bx,by - depth,llx,lly, bx,by - depth,llx,lly - depth };
    prim(GL_TRIANGLES, fl, 6, rs, gs, bs, a);
    float fr[] = { bx,by,rx,ry,rx,ry - depth,bx,by, rx,ry - depth,bx,by - depth };
    prim(GL_TRIANGLES, fr, 6, rs * .75f, gs * .75f, bs * .75f, a);
}

// ============================================================
//  Tiles visuais
// ============================================================
static void tParede(float lx, float ly) {
    isoTile(lx, ly, .22f, .22f, .38f, .10f, .10f, .20f);
    float cx = lx + TILE_W / 2.f, cy = ly + TILE_H / 2.f + 4;
    quad(cx - 14, cy - 4, 12, 8, .30f, .30f, .50f);
    quad(cx + 3, cy - 4, 12, 8, .30f, .30f, .50f);
}
static void tChao(float lx, float ly) {
    isoTile(lx, ly, .18f, .45f, .28f, .09f, .23f, .13f);
}
static void tAgua(float lx, float ly) {
    float w = .3f + .08f * sinf(tempo * 2.5f);
    isoTile(lx, ly, .10f, .35f + w, .70f + w, .05f, .16f, .38f);
    float cx = lx + TILE_W / 2.f, cy = ly + TILE_H / 2.f + 2;
    float onda[] = { cx - 18,cy + 2,cx - 8,cy + 5,cx + 2,cy + 2 };
    prim(GL_LINE_STRIP, onda, 3, .6f, .85f, 1.f, .5f);
}
static void tAreia(float lx, float ly) {
    isoTile(lx, ly, .80f, .72f, .30f, .48f, .42f, .15f);
    float cx = lx + TILE_W / 2.f, cy = ly + TILE_H / 2.f;
    circle(cx - 10, cy + 3, 2, .92f, .82f, .40f);
    circle(cx + 8, cy + 5, 2, .88f, .78f, .36f);
}
static void tGrama(float lx, float ly) {
    isoTile(lx, ly, .28f, .72f, .22f, .14f, .38f, .10f);
    float cx = lx + TILE_W / 2.f, cy = ly + TILE_H / 2.f + 2;
    tri(cx - 12, cy, cx - 10, cy, cx - 11, cy + 9, .20f, .60f, .15f);
    tri(cx, cy, cx + 2, cy, cx + 1, cy + 9, .22f, .62f, .17f);
    tri(cx + 10, cy, cx + 12, cy, cx + 11, cy + 9, .20f, .60f, .15f);
}
static void tLava(float lx, float ly) {
    float p = .4f + .25f * sinf(tempo * 3.f);
    isoTile(lx, ly, .80f, .18f + p * .1f, .02f, .42f, .08f, .00f);
    float cx = lx + TILE_W / 2.f, cy = ly + TILE_H / 2.f;
    circle(cx - 10, cy + 3, 4 + p * 2, .95f, .45f, .05f, .7f);
    circle(cx + 10, cy - 2, 3 + p * 2, .90f, .30f, .00f, .6f);
}
static void tGelo(float lx, float ly) {
    float sh = .05f * sinf(tempo * 2.f);
    isoTile(lx, ly, .72f, .88f + sh, 1.f, .38f, .52f, .70f);
    float cx = lx + TILE_W / 2.f, cy = ly + TILE_H / 2.f + 4;
    quad(cx - 12, cy - 2, 24, 3, 1.f, 1.f, 1.f, .35f);
}
static void tPilar(float lx, float ly) {
    tChao(lx, ly);
    float cx = lx + TILE_W / 2.f, cy = ly + TILE_H / 2.f + TILE_H * .20f;
    quad(cx - 9, cy - 8, 18, 22, .25f, .25f, .42f);
    quad(cx - 13, cy - 8, 26, 7, .33f, .33f, .54f);
    quad(cx - 13, cy + 12, 26, 7, .33f, .33f, .54f);
}
static void tSaida(float lx, float ly) {
    float p = .5f + .4f * sinf(tempo * 4.f);
    isoTile(lx, ly, .10f, p * .9f, .10f, .04f, .30f, .04f);
    float cx = lx + TILE_W / 2.f, cy = ly + TILE_H / 2.f + 4;
    tri(cx, cy + 14, cx - 10, cy + 2, cx + 10, cy + 2, .8f, 1.f, .8f, .9f);
    quad(cx - 4, cy - 8, 8, 12, .8f, 1.f, .8f, .9f);
}

// ============================================================
//  Sprite do jogador ANIMADO
//  Partes: sombra, pernas (oscilam ao andar), corpo, cabeca,
//          bracos (balancem), olhos (direcionais)
//  Tecnica identica ao boneco do Modulo 5: partes geometricas
//  que se movem em funcao de um angulo/fase de animacao
// ============================================================
static void desenhaJogador(float lx, float ly) {
    float cx = lx + TILE_W / 2.f;
    float cy = ly + TILE_H / 2.f + 8;

    // piscando durante invencibilidade
    if (jog_invincivel > 0 && (jog_invincivel / 4) % 2 == 0) return;

    // Sombra eliptica no chao
    for (int i = 6; i >= 1; i--)
        circle(cx, cy - 14, (float)i * 2.f, .0f, .0f, .0f, .04f * (7 - i));

    // Pernas animadas (oscilam ao andar)
    float perna_offset = jog_andando ? sinf(anim_pernas) * 7.f : 0.f;
    quad(cx - 8, cy - 18, 7, 14, .15f, .35f, .80f);
    quad(cx - 8, cy - 18 + perna_offset, 7, 6, .12f, .28f, .65f);
    quad(cx + 1, cy - 18, 7, 14, .15f, .35f, .80f);
    quad(cx + 1, cy - 18 - perna_offset, 7, 6, .12f, .28f, .65f);

    // Corpo
    quad(cx - 10, cy - 6, 20, 22, .20f, .48f, .92f);
    quad(cx - 10, cy + 6, 20, 4, .10f, .28f, .60f);

    // Bracos animados (balancam opostos as pernas)
    float braco_offset = jog_andando ? sinf(anim_pernas + 3.14f) * 5.f : 0.f;
    quad(cx - 18, cy - 4 + braco_offset, 8, 16, .20f, .48f, .92f);
    quad(cx + 10, cy - 4 - braco_offset, 8, 16, .20f, .48f, .92f);

    // Cabeca
    circle(cx, cy + 18, 13, .28f, .58f, 1.0f);
    quad(cx - 13, cy + 24, 26, 8, .10f, .20f, .50f);

    // Olhos direcionais
    float ex = 0, ey = 0;
    switch (jog_dir) {
    case 0: ex = 5;ey = 0;break;
    case 1: ex = 4;ey = -3;break;
    case 2: ex = 0;ey = -4;break;
    case 3: ex = -4;ey = -3;break;
    case 4: ex = -5;ey = 0;break;
    case 5: ex = -4;ey = 3;break;
    case 6: ex = 0;ey = 4;break;
    case 7: ex = 4;ey = 3;break;
    }
    circle(cx + ex - 4, cy + 18 + ey, 4.f, 1, 1, 1);
    circle(cx + ex + 4, cy + 18 + ey, 4.f, 1, 1, 1);
    circle(cx + ex - 4, cy + 18 + ey, 2.f, .0f, .0f, .85f);
    circle(cx + ex + 4, cy + 18 + ey, 2.f, .0f, .0f, .85f);

    // Indicador de direcao
    float ax = cx + ex * 2, ay = cy - 22;
    tri(ax, ay, ax - 5, ay + 8, ax + 5, ay + 8, .9f, 1.f, .9f, .7f);
}

// ============================================================
//  Moedas (itens colecionaveis)
// ============================================================
static void desenhaMoeda(float lx, float ly) {
    float cx = lx + TILE_W / 2.f;
    float cy = ly + TILE_H / 2.f + 6 + sinf(tempo * 3.f) * 3.f;
    float r = 7.f + sinf(tempo * 4.f) * 1.f;

    circle(cx, cy, r, 1.f, .85f, .0f);
    circle(cx, cy, r * .6f, .9f, .75f, .0f);
    circle(cx - 2, cy + 2, r * .25f, 1.f, 1.f, .8f, .8f);
}

// ============================================================
//  HUD
// ============================================================
static void desenhaHUD() {
    quad(0, WIN_H - HUD_H, WIN_W, HUD_H, .04f, .04f, .10f, .95f);
    float lb[] = { 0,(float)(WIN_H - HUD_H),(float)WIN_W,(float)(WIN_H - HUD_H) };
    prim(GL_LINES, lb, 2, .18f, .18f, .32f);

    // Vidas
    float vx = 20.f;
    for (int i = 0; i < 3; i++) {
        float alpha = (i < jog_vidas) ? 1.f : 0.25f;
        circle(vx + 6, WIN_H - HUD_H / 2.f + 6, 7, .9f, .15f, .15f, alpha);
        circle(vx + 18, WIN_H - HUD_H / 2.f + 6, 7, .9f, .15f, .15f, alpha);
        tri(vx, WIN_H - HUD_H / 2.f + 6,
            vx + 24, WIN_H - HUD_H / 2.f + 6,
            vx + 12, WIN_H - HUD_H / 2.f - 8,
            .9f, .15f, .15f, alpha);
        vx += 36.f;
    }

    // Moedas coletadas
    float mx = 140.f;
    for (int i = 0; i < num_itens; i++) {
        float alpha = (i < jog_moedas) ? 1.f : 0.25f;
        circle(mx, WIN_H - HUD_H / 2.f + 2, 8, 1.f, .85f, .0f, alpha);
        mx += 20.f;
    }

    // Mensagem contextual
    if (msg_timer > 0) {
        float alpha = (msg_timer < 40) ? msg_timer / 40.f : 1.f;
        int len = strlen(mensagem);
        float tx = WIN_W / 2.f - len * 3.5f;
        quad(tx - 8, WIN_H - HUD_H / 2.f - 14, len * 7.f + 16, 22,
            .0f, .0f, .0f, .65f * alpha);
        for (int i = 0; i < len; i++) {
            if (mensagem[i] != ' ')
                quad(tx + i * 7.f, WIN_H - HUD_H / 2.f - 10, 5, 14,
                    .95f, .88f, .30f, alpha);
        }
    }

    if (estado_jogo == ESTADO_VITORIA) {
        quad(WIN_W / 2.f - 200, WIN_H / 2.f - 40, 400, 80, .0f, .3f, .0f, .85f);
        for (int i = 0; i < 12; i++)
            quad(WIN_W / 2.f - 180 + i * 30.f, WIN_H / 2.f - 10, 22, 28, .4f, 1.f, .4f);
    }

    if (estado_jogo == ESTADO_GAME_OVER) {
        quad(WIN_W / 2.f - 200, WIN_H / 2.f - 40, 400, 80, .3f, .0f, .0f, .85f);
        for (int i = 0; i < 10; i++)
            quad(WIN_W / 2.f - 160 + i * 32.f, WIN_H / 2.f - 10, 24, 28, 1.f, .3f, .3f);
    }
}

// ============================================================
//  Leitura dos arquivos de configuracao
// ============================================================
void carregarTileset(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        printf("tileset.txt nao encontrado, usando padrao\n");
        return;
    }
    std::string linha;
    while (std::getline(f, linha)) {
        if (linha.empty() || linha[0] == '#') continue;
        break;
    }
    int id; std::string nome; int walk;
    while (f >> id >> nome >> walk) {
        if (id >= 0 && id < 9) walkable[id] = walk;
    }
    printf("tileset.txt carregado\n");
}

void carregarMapa(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        printf("map.txt nao encontrado, usando mapa padrao\n");
        itensDefault();
        return;
    }

    std::string linha;
    int cols = 0, lins = 0;

    while (std::getline(f, linha)) {
        if (linha.empty() || linha[0] == '#') continue;
        std::istringstream ss(linha);
        ss >> cols >> lins;
        break;
    }

    int count = 0;
    while (count < lins && std::getline(f, linha)) {
        if (linha.empty() || linha[0] == '#') continue;
        std::istringstream ss(linha);
        int id;
        for (int c = 0; c<cols && ss >> id; c++) {
            if (count < LINHAS && c < COLUNAS)
                mapa[count][c] = id;
        }
        count++;
    }

    bool achouItems = false;
    while (std::getline(f, linha)) {
        if (linha.find("WALKABLE") != std::string::npos) break;
        if (linha.find("ITEMS") != std::string::npos) { achouItems = true; break; }
    }

    if (!achouItems) {
        int id2, walk;
        while (std::getline(f, linha)) {
            if (linha.empty() || linha[0] == '#') continue;
            if (linha.find("ITEMS") != std::string::npos) { achouItems = true; break; }
            std::istringstream ss(linha);
            if (ss >> id2 >> walk && id2 >= 0 && id2 < 9)
                walkable[id2] = walk;
        }
    }

    num_itens = 0;
    while (std::getline(f, linha)) {
        if (linha.empty() || linha[0] == '#') continue;
        char tipo; int col, lin;
        std::istringstream ss(linha);
        if (ss >> tipo >> col >> lin) {
            if (tipo == 'C' && num_itens < MAX_ITENS)
                itens[num_itens++] = { col, lin, 0, 1 };
        }
    }
    if (num_itens == 0) itensDefault();
    printf("map.txt carregado: %dx%d, %d itens\n", cols, lins, num_itens);
}

// ============================================================
//  Logica
// ============================================================
struct Dir { int dc, dl, idx; };
static const Dir DIRS[8] = {
    { 1, 0, 0}, { 1, 1, 1}, { 0, 1, 2}, {-1, 1, 3},
    {-1, 0, 4}, {-1,-1, 5}, { 0,-1, 6}, { 1,-1, 7},
};

static void mover(int dirIdx) {
    if (estado_jogo != ESTADO_JOGANDO) return;
    const Dir& d = DIRS[dirIdx];
    jog_dir = d.idx;
    int nc = jog_col + d.dc;
    int nl = jog_lin + d.dl;

    if (nc < 0 || nc >= COLUNAS || nl < 0 || nl >= LINHAS) return;
    int t = mapa[nl][nc];
    if (!walkable[t]) {
        snprintf(mensagem, sizeof(mensagem), "Caminho bloqueado!");
        msg_timer = 40; return;
    }

    jog_col = nc; jog_lin = nl;
    jog_andando = 1;

    // Tile swap: ao pisar, chao vira areia (marca visual de que foi pisado)
    if (mapa[nl][nc] == T_CHAO)
        mapa[nl][nc] = T_AREIA;

    // Lava: tira uma vida
    if (t == T_LAVA && jog_invincivel == 0) {
        jog_vidas--;
        jog_invincivel = 90;
        snprintf(mensagem, sizeof(mensagem), "LAVA! Perdeu uma vida!");
        msg_timer = 80;
        if (jog_vidas <= 0) {
            estado_jogo = ESTADO_GAME_OVER;
            snprintf(mensagem, sizeof(mensagem), "GAME OVER! R para reiniciar");
            msg_timer = 999;
        }
        return;
    }

    // Saida: so pode entrar se coletou todas as moedas
    if (t == T_SAIDA) {
        if (jog_moedas >= num_itens) {
            estado_jogo = ESTADO_VITORIA;
            snprintf(mensagem, sizeof(mensagem), "VOCE VENCEU!");
            msg_timer = 999;
        }
        else {
            snprintf(mensagem, sizeof(mensagem), "Colete %d moedas antes!", num_itens);
            msg_timer = 80;
        }
        return;
    }

    // Coleta de moedas
    for (int i = 0; i < num_itens; i++) {
        if (!itens[i].ativo) continue;
        if (itens[i].col == jog_col && itens[i].lin == jog_lin) {
            itens[i].ativo = 0;
            jog_moedas++;
            snprintf(mensagem, sizeof(mensagem), "Moeda! %d/%d coletadas", jog_moedas, num_itens);
            msg_timer = 60;
        }
    }
}

static void reiniciar() {
    jog_col = 1; jog_lin = 1; jog_dir = 0;
    jog_vidas = 3; jog_moedas = 0; jog_invincivel = 0;
    estado_jogo = ESTADO_JOGANDO;
    for (int i = 0; i < num_itens; i++) itens[i].ativo = 1;

    int mapa_orig[LINHAS][COLUNAS] = {
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
        {0,1,1,1,1,2,2,1,1,1,1,1,6,6,6,1,1,0},
        {0,1,7,1,1,2,2,1,4,4,4,1,6,7,6,1,1,0},
        {0,1,1,3,3,1,1,4,4,4,4,1,6,6,6,3,1,0},
        {0,1,1,3,7,1,1,4,7,4,4,1,1,1,3,3,1,0},
        {0,1,5,5,1,1,1,4,4,4,1,1,1,3,3,7,1,0},
        {0,1,5,5,5,1,1,1,1,1,1,1,1,1,3,3,1,0},
        {0,1,5,7,5,1,0,0,1,1,1,0,0,1,1,1,1,0},
        {0,1,1,1,1,1,0,0,1,2,1,0,0,1,6,6,1,0},
        {0,0,1,1,1,0,0,1,1,2,1,1,0,1,6,7,1,0},
        {0,0,0,1,1,0,1,1,1,1,1,1,1,1,6,6,1,0},
        {0,1,1,1,1,1,1,7,1,1,1,7,1,1,1,1,1,0},
        {0,1,3,3,1,5,5,1,1,1,1,1,5,5,1,4,4,0},
        {0,1,3,1,1,5,1,1,1,1,1,1,1,5,1,4,1,0},
        {0,1,1,1,1,1,1,1,2,2,2,1,1,1,1,1,1,0},
        {0,1,6,6,1,1,1,1,2,8,2,1,1,1,6,6,1,0},
        {0,1,6,1,1,1,1,1,2,2,2,1,1,1,1,6,1,0},
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    };
    memcpy(mapa, mapa_orig, sizeof(mapa));
    snprintf(mensagem, sizeof(mensagem), "Colete todas as moedas e chegue a SAIDA!");
    msg_timer = 180;
}

// ============================================================
//  Render
// ============================================================
static void render() {
    glClearColor(.04f, .04f, .10f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Painter's algorithm: percorre por diagonais (fundo -> frente)
    for (int diag = 0; diag < LINHAS + COLUNAS - 1; diag++) {
        for (int lin = 0; lin < LINHAS; lin++) {
            int col = diag - lin;
            if (col < 0 || col >= COLUNAS) continue;

            float lx = isoX(col, lin) - TILE_W / 2.f;
            float ly = isoY(col, lin) - TILE_H / 2.f;

            switch (mapa[lin][col]) {
            case T_PAREDE: tParede(lx, ly); break;
            case T_CHAO:   tChao(lx, ly);   break;
            case T_AGUA:   tAgua(lx, ly);    break;
            case T_AREIA:  tAreia(lx, ly);   break;
            case T_GRAMA:  tGrama(lx, ly);   break;
            case T_LAVA:   tLava(lx, ly);    break;
            case T_GELO:   tGelo(lx, ly);    break;
            case T_PILAR:  tPilar(lx, ly);   break;
            case T_SAIDA:  tSaida(lx, ly);   break;
            }

            for (int i = 0; i < num_itens; i++) {
                if (itens[i].ativo && itens[i].col == col && itens[i].lin == lin)
                    desenhaMoeda(lx, ly);
            }

            if (jog_col == col && jog_lin == lin)
                desenhaJogador(lx, ly);
        }
    }

    desenhaHUD();
    glDisable(GL_BLEND);
}

// ============================================================
//  Callbacks GLFW
// ============================================================
static void keyCallback(GLFWwindow* win, int key, int sc, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    if (key == GLFW_KEY_ESCAPE) { glfwSetWindowShouldClose(win, GLFW_TRUE); return; }
    if (key == GLFW_KEY_R) { reiniciar(); return; }

    switch (key) {
    case GLFW_KEY_D: case GLFW_KEY_RIGHT:     mover(0); break; // E
    case GLFW_KEY_C:                           mover(1); break; // SE
    case GLFW_KEY_S: case GLFW_KEY_DOWN:       mover(2); break; // S
    case GLFW_KEY_Z:                           mover(3); break; // SW
    case GLFW_KEY_A: case GLFW_KEY_LEFT:       mover(4); break; // W
    case GLFW_KEY_Q:                           mover(5); break; // NW
    case GLFW_KEY_W: case GLFW_KEY_UP:         mover(6); break; // N
    case GLFW_KEY_E:                           mover(7); break; // NE
    }
}

static void framebufferCB(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
    ortho2D(projMatrix, 0, (float)w, 0, (float)h);
}

// ============================================================
//  main
// ============================================================
int main() {
    if (!glfwInit()) { fprintf(stderr, "GLFW falhou\n"); return 1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

    GLFWwindow* win = glfwCreateWindow(WIN_W, WIN_H,
        "GRAU B", NULL, NULL);
    if (!win) { fprintf(stderr, "Janela falhou\n"); glfwTerminate(); return 1; }

    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "GLAD falhou\n"); return 1;
    }
    printf("OpenGL: %s\n", glGetString(GL_VERSION));

    carregarTileset("config/tileset.txt");
    carregarMapa("config/map.txt");

    buildShaders();
    setupVAO();
    ortho2D(projMatrix, 0, WIN_W, 0, WIN_H);
    glViewport(0, 0, WIN_W, WIN_H);

    glfwSetKeyCallback(win, keyCallback);
    glfwSetFramebufferSizeCallback(win, framebufferCB);

    printf("\n=== CEMITERIO SOMBRIO ===\n");
    printf("Objetivo: Colete %d moedas e chegue a SAIDA (verde)\n", num_itens);
    printf("Cuidado: LAVA tira vidas!\n");
    printf("Controles:\n");
    printf("  Q=NW  W=N  E=NE\n");
    printf("  A=W        D=E\n");
    printf("  Z=SW  S=S  C=SE\n");
    printf("  R=Reiniciar  ESC=Sair\n\n");

    double last = glfwGetTime();
    while (!glfwWindowShouldClose(win)) {
        double now = glfwGetTime();
        float dt = (float)(now - last); last = now;
        tempo += dt;

        if (jog_invincivel > 0) jog_invincivel--;
        if (msg_timer > 0) msg_timer--;

        if (jog_andando) {
            anim_pernas += 0.25f;
            jog_andando = 0;
        }

        render();
        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}