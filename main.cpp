/*******************************************************************************************
*
*
*   3 regles simples :
*   Separation (si un oiseau est trop proche d'un de ses voisin il s'en ecarte),
*   Alignement (ils s'alignent dans la direction des oiseau qui l'entourent),
*   Cohésion (cohésion pour aller vers la position moyenne des oiseau qui l'entourent).
*
*   
*
********************************************************************************************/
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include <stdlib.h>
#include <vector>

#include "sol.h"
#define RLIGHTS_IMPLEMENTATION
#if defined(_WIN32) || defined(_WIN64)
#include "include/shaders/rlights.h"
#elif defined(__linux__)
#include "include/shaders/rlights.h"
#endif

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            330//120//si c'est 100 ça ouvre pas les autres shaders
#endif

#define SHADOWMAP_RESOLUTION 4096 //la resolution de la shadowmap
#define NB_OISEAUX 10

class Oiseau
{
    public:
    Model model;
    Vector3 position;
    Vector3 direction;
    float vitesse;
    float angle;
    float espacement;
    Oiseau(Model model, Vector3 position, Vector3 direction, float vitesse, float angle, float espacement)
    {
        this->model = model;
        this->position = position;
        this->direction = direction;
        this->vitesse = vitesse;
        this->angle = angle;
        this->espacement = espacement;
    }
    
    void draw()
    {
        DrawModel(this->model, this->position, 1.10f, BLACK);
    }
};

//les ombres
//by @TheManTheMythTheGameDev
RenderTexture2D LoadShadowmapRenderTexture(int width, int height);
void UnloadShadowmapRenderTexture(RenderTexture2D target);


RenderTexture2D LoadShadowmapRenderTexture(int width, int height)
{
    RenderTexture2D target = { 0 };

    target.id = rlLoadFramebuffer(); // Load an empty framebuffer
    target.texture.width = width;
    target.texture.height = height;

    if (target.id > 0)
    {
        rlEnableFramebuffer(target.id);

        // Create depth texture
        // We don't need a color texture for the shadowmap
        target.depth.id = rlLoadTextureDepth(width, height, false);
        target.depth.width = width;
        target.depth.height = height;
        target.depth.format = 19;       //DEPTH_COMPONENT_24BIT?
        target.depth.mipmaps = 1;

        // Attach depth texture to FBO
        rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

        // Check if fbo is complete with attachments (valid)
        if (rlFramebufferComplete(target.id)) TRACELOG(LOG_INFO, "FBO: [ID %i] Framebuffer object created successfully", target.id);

        rlDisableFramebuffer();
    }
    else TRACELOG(LOG_WARNING, "FBO: Framebuffer object can not be created");

    return target;
}

// Unload shadowmap render texture from GPU memory (VRAM)
void UnloadShadowmapRenderTexture(RenderTexture2D target)
{
    if (target.id > 0)
    {
        // NOTE: Depth texture/renderbuffer is automatically
        // queried and deleted before deleting framebuffer
        rlUnloadFramebuffer(target.id);
    }
}

//fonction pour simuler le vol d'une nuée d'oiseaux
void update_oiseaux(std::vector<Oiseau>& oiseaux, float dt, float espacement, float vitesse){
    for (auto& oiseau : oiseaux) {
        // Règles de déplacement du boid (flocking algorithm)
        Vector3 separation = {0}; // Éviter les collisions
        Vector3 alignment = {0};  // Aligner sa direction avec les voisins
        Vector3 cohesion = {0};   // Aller vers le centre de masse des voisins
        // Additional rule: return to center if too far away
        Vector3 centerAttraction = {0};
        float maxDistanceFromCenter = 15.0f;
        float distanceFromCenter = Vector3Length(oiseau.position);
                
        // Apply stronger force as birds get farther from center
        if (distanceFromCenter > maxDistanceFromCenter * 0.5f) {
            float centerForce = (distanceFromCenter / maxDistanceFromCenter) * 0.5f;
            centerAttraction = Vector3Scale(Vector3Normalize(Vector3Negate(oiseau.position)), centerForce);
        }
        
        int neighborCount = 0;
        
        // Paramètres d'influence des règles
        const float separationWeight = 1.5f;
        const float alignmentWeight = 1.0f;
        const float cohesionWeight = 0.8f;
        
        // Examiner tous les autres oiseaux
        for (const auto& autre : oiseaux) {
            // Éviter de se comparer à soi-même
            if (&autre == &oiseau) continue;
            
            Vector3 diff = Vector3Subtract(oiseau.position, autre.position);
            float distance = Vector3Length(diff);
            
            // Ne considérer que les oiseaux dans un certain rayon
            if (distance < espacement * 3.0f) {
                // Règle de séparation - s'éloigner des oiseaux trop proches
                if (distance < espacement) {
                    separation = Vector3Add(separation, 
                                  Vector3Scale(Vector3Normalize(diff), 
                                  1.0f / fmaxf(distance, 0.1f)));
                }
                
                // Règle d'alignement et de cohésion pour les oiseaux proches
                alignment = Vector3Add(alignment, autre.direction);
                cohesion = Vector3Add(cohesion, autre.position);
                
                neighborCount++;
            }
        }
        
        // Calculer les vecteurs moyens
        if (neighborCount > 0) {
            // Normaliser l'alignement (moyenne des directions)
            alignment = Vector3Scale(alignment, 1.0f/neighborCount);
            
            // Normaliser la cohésion (aller vers le centre de masse)
            cohesion = Vector3Scale(cohesion, 1.0f/neighborCount);
            cohesion = Vector3Subtract(cohesion, oiseau.position);
        }
        
        // Appliquer les forces avec leurs poids respectifs
        Vector3 acceleration = {0};
        acceleration = Vector3Add(acceleration, Vector3Scale(separation, separationWeight));
        acceleration = Vector3Add(acceleration, Vector3Scale(alignment, alignmentWeight));
        acceleration = Vector3Add(acceleration, Vector3Scale(cohesion, cohesionWeight));
        
        // Mettre à jour la direction (limiter la rotation maximale)
        if (Vector3Length(acceleration) > 0) {
            Vector3 newDirection = Vector3Add(oiseau.direction, Vector3Scale(acceleration, dt));
            oiseau.direction = Vector3Normalize(newDirection);
        }
        
        // Mettre à jour la position
        oiseau.position = Vector3Add(oiseau.position, Vector3Scale(oiseau.direction, vitesse * dt));
    }
}

//fonction pour dessiner la scene
void draw_scene(Camera camera, std::vector<Oiseau> oiseaux, int espacement, float vitesse, Model sol){
    BeginMode3D(camera);
    for (int i = 0; i < oiseaux.size(); i++)
    {
        oiseaux[i].draw();
    }
    DrawModel(sol, Vector3Zero(), 1.0f, WHITE);
    DrawGrid(20, 1.0f);
    EndMode3D();
}

int main(void) {
    // Initialisation
    const int screenWidth = 1280;//1920;
    const int screenHeight = 720;//1080;
    SetConfigFlags(FLAG_MSAA_4X_HINT); // Enable Multi Sampling Anti Aliasing 4x (if available)

    InitWindow(screenWidth, screenHeight, "raylib - Projet tutore");
    rlDisableBackfaceCulling();//pour voir l'arriere des objets
    /*
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableScissorTest();
    rlEnableColorBlend();
    glEnable(GL_DEPTH_TEST);
    */
    //rlEnableDepthTest();    // Activer le test de profondeur
    //rlEnableDepthMask();    // Activer l'écriture dans le tampon de profondeur
    //glBlendFunc(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA);
    rlEnableColorBlend(); // Activer le blending
    rlSetBlendMode(RL_BLEND_ALPHA);

    // Caméra pour visualiser la scène
    Camera camera = { 
    .position = (Vector3){ 5.0f, 5.0f, 5.0f },
    .target = (Vector3){ 0.0f, 0.0f, 0.0f },
    .up = (Vector3){ 0.0f, 1.0f, 0.0f },
    .fovy = 80.0f,
    .projection = CAMERA_PERSPECTIVE,
    };

    //Lumière directionnelle
    // Load basic lighting shader
    Shader shader = LoadShader(TextFormat("include/shaders/resources/shaders/glsl%i/lighting.vs", GLSL_VERSION),TextFormat("include/shaders/resources/shaders/glsl%i/lighting.fs", GLSL_VERSION));
    //les ombres
    Shader shadowShader = LoadShader(TextFormat("include/shaders/resources/shaders/glsl%i/shadowmap.vs", GLSL_VERSION),TextFormat("include/shaders/resources/shaders/glsl%i/shadowmap.fs", GLSL_VERSION));

    shadowShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shadowShader, "viewPos");

    
    Vector3 lightDir = Vector3Normalize((Vector3){ 0.35f, -1.0f, -0.35f });
    Color lightColor = WHITE;
    Vector4 lightColorNormalized = ColorNormalize(lightColor);
    int lightDirLoc = GetShaderLocation(shadowShader, "lightDir");
    int lightColLoc = GetShaderLocation(shadowShader, "lightColor");
    SetShaderValue(shadowShader, lightDirLoc, &lightDir, SHADER_UNIFORM_VEC3);
    SetShaderValue(shadowShader, lightColLoc, &lightColorNormalized, SHADER_UNIFORM_VEC4);
    
    int ambientLoc = GetShaderLocation(shadowShader, "ambient");
    float ambient[4] = {0.1f, 0.1f, 0.1f, 1.0f};
    SetShaderValue(shadowShader, ambientLoc, ambient, SHADER_UNIFORM_VEC4);
    int lightVPLoc = GetShaderLocation(shadowShader, "lightVP");
    int shadowMapLoc = GetShaderLocation(shadowShader, "shadowMap");
    int shadowMapResolution = SHADOWMAP_RESOLUTION;
    SetShaderValue(shadowShader, GetShaderLocation(shadowShader, "shadowMapResolution"), &shadowMapResolution, SHADER_UNIFORM_INT);

    //modele de l'oiseau
    Model model_oiseau  = LoadModel("ressources/flying_bird.glb");
    model_oiseau.materials[0].shader = shadowShader;
    for (int i = 0; i < model_oiseau.materialCount; i++)
    {
        model_oiseau.materials[i].shader = shadowShader;
    }
    model_oiseau.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    //la shadowmap
    RenderTexture2D shadowMap = LoadShadowmapRenderTexture(SHADOWMAP_RESOLUTION, SHADOWMAP_RESOLUTION);
    
    //la light camera
    Camera3D lightCam = { 0 };
    lightCam.position = Vector3Scale(lightDir, -15.0f);
    lightCam.target = Vector3Zero();
    lightCam.projection = CAMERA_ORTHOGRAPHIC;
    lightCam.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    lightCam.fovy = 20.0f;

    
    
    DisableCursor();// Limit cursor to relative movement inside the window

    SetTargetFPS(165);
    Mesh sphere_test = GenMeshSphere(1.0f, 16, 16);
    Material material_test = LoadMaterialDefault();
    material_test.shader = shadowShader;
    material_test.maps[MATERIAL_MAP_DIFFUSE].color = RED;

    Mesh sol = GenMeshPlane(20.0f, 20.0f, 10, 10);
    Material material_sol = LoadMaterialDefault();
    material_sol.shader = shadowShader;
    material_sol.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    Model model_sol = LoadModelFromMesh(sol);

    float espacement = 2.0f;
    float vitesse = 3.0f;
    // Initialisation des oiseaux
    std::vector<Oiseau> oiseaux;
    for (int i = 0; i < NB_OISEAUX; i++)
    {
        Vector3 position = { GetRandomValue(-5, 5), GetRandomValue(-5, 5), GetRandomValue(-5, 5) };
        Vector3 direction = { GetRandomValue(-1, 1), GetRandomValue(-1, 1), GetRandomValue(-1, 1) };
        float angle = 0.0f;
        espacement = 2.0f;
        oiseaux.push_back(Oiseau(model_oiseau, position, direction, vitesse, angle, espacement));
    }
    float timeOfDay = 12.0f;
    // Boucle principale
    while (!WindowShouldClose()) {
        UpdateCamera(&camera, CAMERA_ORBITAL);
        // Update the shader with the camera view vector (points towards { 0.0f, 0.0f, 0.0f })
        Vector3 cameraPos = camera.position;
        SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], &cameraPos, SHADER_UNIFORM_VEC3);
        float dt = GetFrameTime();
        
        //DisableCursor();//pour pas avoir le curseur qui sort de l'ecran
        ShowCursor();//pour voir le curseur
        
        
        float sunAngle = ((timeOfDay - 6.0f) / 12.0f) * PI; // -PI/2 à PI/2 (6h à 18h)

        // Calculer la direction de la lumière (normalisée)
        Vector3 lightDir = {
            cosf(sunAngle),           // X: Est-Ouest
            -sinf(sunAngle),          // Y: Hauteur du soleil
            0.0f                      // Z: Nord-Sud
        };
        lightDir = Vector3Normalize(lightDir);
        // Mise à jour de la lumière directionnelle
        //directionalLight.position = Vector3Scale(lightDir, -1.0f); // Inverse la direction pour pointer vers la source
        //directionalLight.target = Vector3Zero();
        //directionalLight.color = GetSunColor(timeOfDay);

        //on bouge light dir aussi
        //lightDir = Vector3Normalize((Vector3){ cosf(sunAngle), -sinf(sunAngle), 0.0f });

        //on bouge aussi la camera de lumière
        lightCam.position = Vector3Scale(lightDir, -15.0f);
        lightCam.target = Vector3Zero();
        
        SetShaderValue(shadowShader, lightDirLoc, &lightDir, SHADER_UNIFORM_VEC3);

        // Rendu final (vue normale)
        BeginDrawing();
        //on dessine les ombres ici
        Matrix lightView;
        Matrix lightProj;
        BeginTextureMode(shadowMap);
        ClearBackground(SKYBLUE);

        BeginMode3D(lightCam);
            lightView = rlGetMatrixModelview();
            lightProj = rlGetMatrixProjection();
            update_oiseaux(oiseaux, GetFrameTime(), espacement, vitesse);
            draw_scene(camera, oiseaux, espacement, vitesse, model_sol);
        EndMode3D();
        EndTextureMode();
        Matrix lightViewProj = MatrixMultiply(lightView, lightProj);

        ClearBackground(SKYBLUE);

        SetShaderValueMatrix(shadowShader, lightVPLoc, lightViewProj);

        rlEnableShader(shadowShader.id);

        int slot = 10;
        rlActiveTextureSlot(10);
        rlEnableTexture(shadowMap.depth.id);
        rlSetUniform(shadowMapLoc, &slot, SHADER_UNIFORM_INT, 1);

        BeginMode3D(camera);
            update_oiseaux(oiseaux, GetFrameTime(), espacement, vitesse);
            draw_scene(camera, oiseaux, espacement, vitesse, model_sol);
            //DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 1.0f, 1.0f, 1.0f, RED);
            DrawGrid(20, 1.0f);

        EndMode3D();
        //BeginShaderMode(shader);

        //update la lumière
        //UpdateLightValues(shader, directionalLight);

        //EndShaderMode();
        DrawGrid(20, 1.0f);
        EndMode3D();
        // Affichage de l'interface utilisateur
        DrawText(" d'objets 3D - Utilisez la souris pour naviguer", 10, 10, 20, DARKGRAY);
        DrawText("Maintenez le clic droit pour tourner la scène", 10, 25, 20, DARKGRAY);
        DrawFPS(10, 40);

        /*
        l'ui pour controler les paramètres
        */
        // Pour changer la direction de la lumière
        GuiSliderBar((Rectangle){ 100, 100, 200, 20 }, "Time of Day", TextFormat("%.0f:00", timeOfDay), &timeOfDay, 0.0f, 24.0f);
        //pour changer la vitessse
        GuiSliderBar((Rectangle){ 100, 130, 200, 20 }, "Vitesse", TextFormat("%.2f", vitesse), &vitesse, 0.0f, 10.0f);
        //pour changer l'espacement
        GuiSliderBar((Rectangle){ 100, 160, 200, 20 }, "Espacement", TextFormat("%.2f", espacement), &espacement, 0.0f, 10.0f);

        // Affichage de l'heure
        DrawText(TextFormat("Time: %.0f:00", timeOfDay), 310, 10, 20, DARKGRAY);
        EndDrawing();
    }

    UnloadShader(shader);
    UnloadShader(shadowShader);
    // Désallocation des ressources
    UnloadModel(model_oiseau);
    CloseWindow();

    return 0;
}