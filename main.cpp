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
#include <stdlib.h>
#include <vector>

#define RLIGHTS_IMPLEMENTATION
#if defined(_WIN32) || defined(_WIN64)
#include "include/shaders/rlights.h"
#define PLATFORME_LINUX false
#elif defined(__linux__)
#include "include/shaders/rlights.h"
#include "raygui.h"
#define PLATFORME_LINUX true
#endif

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            330//120//si c'est 100 ça ouvre pas les autres shaders
#endif

#define SHADOWMAP_RESOLUTION 4096 //la resolution de la shadowmap
#define NB_OISEAUX 200

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
        if (PLATFORME_LINUX){
            DrawModel(this->model, this->position, 1.10f, WHITE);
        }
        else{
            DrawModel(this->model, this->position, 0.010f, WHITE);
        }
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
void update_oiseaux(std::vector<Oiseau>& oiseaux, float dt, float espacement, float vitesse, float maxDistanceFromCenter){
    for (auto& oiseau : oiseaux) {
        // Règles de déplacement du boid (flocking algorithm)
        Vector3 separation = {0}; // Éviter les collisions
        Vector3 alignment = {0};  // Aligner sa direction avec les voisins
        Vector3 cohesion = {0};   // Aller vers le centre de masse des voisins
        
        // Boundary constraint: keep birds inside a sphere
        Vector3 centerAttraction = {0};
        float distanceFromCenter = Vector3Length(oiseau.position);
            
        // Apply stronger boundary constraints - always active but stronger at boundary
        if (distanceFromCenter > maxDistanceFromCenter * 0.5f) {
            // Calculate how close to the boundary the bird is (0.5 at half radius, 1 at boundary)
            float boundaryProximity = distanceFromCenter / maxDistanceFromCenter;
            
            // More aggressive exponential force with higher base strength
            float repulsionStrength = powf(boundaryProximity, 4.0f) * 3.5f;
            
            // Create force vector pointing back toward center
            centerAttraction = Vector3Scale(
                Vector3Normalize(Vector3Negate(oiseau.position)), 
                repulsionStrength
            );
        }
        
        int neighborCount = 0;
        
        // Paramètres d'influence des règles
        const float separationWeight = 1.5f;
        const float alignmentWeight = 1.0f;
        const float cohesionWeight = 0.8f;
        const float centerAttractionWeight = 1.2f; // Increased weight for boundary constraints
        
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
        
        // Add random movement to create more varied flight patterns
        Vector3 randomMovement = {
            (float)GetRandomValue(-100, 100) / 1000.0f,
            (float)GetRandomValue(-100, 100) / 1000.0f,
            (float)GetRandomValue(-100, 100) / 1000.0f
        };
        
        // Appliquer les forces avec leurs poids respectifs
        Vector3 acceleration = {0};
        acceleration = Vector3Add(acceleration, Vector3Scale(separation, separationWeight));
        acceleration = Vector3Add(acceleration, Vector3Scale(alignment, alignmentWeight));
        acceleration = Vector3Add(acceleration, Vector3Scale(cohesion, cohesionWeight));
        acceleration = Vector3Add(acceleration, Vector3Scale(centerAttraction, centerAttractionWeight));
        acceleration = Vector3Add(acceleration, randomMovement);
        
        // Mettre à jour la direction (limiter la rotation maximale)
        if (Vector3Length(acceleration) > 0) {
            Vector3 newDirection = Vector3Add(oiseau.direction, Vector3Scale(acceleration, dt));
            oiseau.direction = Vector3Normalize(newDirection);
        }
        
        // Mettre à jour la position
        oiseau.position = Vector3Add(oiseau.position, Vector3Scale(oiseau.direction, vitesse * dt));
        
        // Hard boundary check - ensure birds never go outside maximum distance
        if (Vector3Length(oiseau.position) > maxDistanceFromCenter) {
            oiseau.position = Vector3Scale(
                Vector3Normalize(oiseau.position),
                maxDistanceFromCenter * 0.95f
            );
            
            // Reflect direction slightly toward center
            Vector3 towardCenter = Vector3Negate(Vector3Normalize(oiseau.position));
            oiseau.direction = Vector3Normalize(Vector3Add(
                oiseau.direction,
                Vector3Scale(towardCenter, 0.5f)
            ));
        }
        
        // Update rotation angle based on movement direction
        // Calculate rotation angle from direction vector (in radians)
        oiseau.angle = atan2f(oiseau.direction.z, oiseau.direction.x);
        
        // Update model rotation to orient birds in their direction of travel
        oiseau.model.transform = MatrixRotateY(oiseau.angle);
        
        // Add a slight bank angle based on turning
        Vector3 up = {0.0f, 1.0f, 0.0f};
        Vector3 right = Vector3CrossProduct(oiseau.direction, up);
        float bankAngle = 0.3f * Vector3Length(acceleration);
        Matrix bankRotation = MatrixRotate(Vector3Normalize(right), bankAngle);
        oiseau.model.transform = MatrixMultiply(oiseau.model.transform, bankRotation);
        //correction de l'orientation du modele
        //90-degres rotation sur l'axe Y
        Matrix correctOrientation = MatrixRotateY(-PI/2.0f); //sens horaire des aiguilles d'une montre
        //appliquer la rotation
        //oiseau.model.transform = MatrixMultiply(correctOrientation, oiseau.model.transform);
        // Add a 180-degree rotation around Y axis to flip the model direction
        Matrix flipDirection = MatrixRotateY(PI);
        //oiseau.model.transform = MatrixMultiply(oiseau.model.transform, flipDirection);
    }
}

Color GetSunColor(float timeOfDay) {
    return (Color){255, 255, 255, 255};
}

//fonction pour dessiner la scene
void draw_scene(Camera camera, std::vector<Oiseau> oiseaux, int espacement, float vitesse, Model sol){
    BeginMode3D(camera);
    for (int i = 0; i < oiseaux.size(); i++)
    {
        oiseaux[i].draw();
    }
    EndMode3D();
}

int main(void) {
    // Initialisation
    const int screenWidth = 1280;//1920;
    const int screenHeight = 720;//1080;
    SetConfigFlags(FLAG_MSAA_4X_HINT); // Enable Multi Sampling Anti Aliasing 4x (if available)

    InitWindow(screenWidth, screenHeight, "raylib - Projet tutore");
    rlDisableBackfaceCulling();//pour voir l'arriere des objets
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
    bool isRotating = false;
    float angleX = 0.0f;
    float angleY = 0.0f;
    float distance_cam = 5.0f;

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
    Model model_oiseau  = LoadModel("ressources/flying_bird/scene.gltf");
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
    //Les animations
    int animsCount = 0;
    unsigned int animIndex = 0;
    unsigned int animCurrentFrame = 0;
    ModelAnimation *modelAnimations = LoadModelAnimations("ressources/flying_bird/scene.gltf", &animsCount);

    if (animsCount > 0) {
        TraceLog(LOG_INFO, "Loaded %d animations for bird model", animsCount);
    } else {
        TraceLog(LOG_WARNING, "No animations found in bird model");
    }
    
    
    
    DisableCursor();// Limit cursor to relative movement inside the window

    SetTargetFPS(1650);
    Material material_test = LoadMaterialDefault();
    material_test.shader = shadowShader;
    material_test.maps[MATERIAL_MAP_DIFFUSE].color = RED;

    Mesh sol = GenMeshPlane(20.0f, 20.0f, 10, 10);
    Material material_sol = LoadMaterialDefault();
    
    Model model_sol = LoadModelFromMesh(sol);
    model_sol.materials[0].shader = shadowShader;
    material_sol.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    for(int i = 0; i < model_sol.materialCount; i++){
        model_sol.materials[i].shader = shadowShader;
    }

    float espacement = 2.0f;
    float vitesse = 3.0f;
    float maxDistanceFromCenter = 15.0f;
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
        //UpdateCamera(&camera, CAMERA_ORBITAL);
        // Activer/désactiver la rotation avec le clic droit
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) isRotating = true;
        if (IsMouseButtonReleased(MOUSE_RIGHT_BUTTON)) isRotating = false;

        // Capture des mouvements de la souris
        if (isRotating) {
            Vector2 mouseDelta = GetMouseDelta();
            angleX -= mouseDelta.y * 0.2f; // Sensibilité verticale
            angleY -= mouseDelta.x * 0.2f; // Sensibilité horizontale
        }
        // Gestion du zoom avec la molette de la souris
        distance_cam -= GetMouseWheelMove() * 0.5f; // Ajustez le facteur (0.5f) pour contrôler la sensibilité du zoom
        if (distance_cam < 2.0f) distance_cam = 2.0f;   // Distance minimale
        if (distance_cam > 200.0f) distance_cam = 200.0f; // Distance maximale


        // Limiter les angles X pour éviter une rotation complète
        if (angleX > 89.0f) angleX = 89.0f;
        if (angleX < -89.0f) angleX = -89.0f;

        // Calcul de la position de la caméra en coordonnées sphériques
        float radAngleX = DEG2RAD * angleX;
        float radAngleY = DEG2RAD * angleY;

        camera.position.x = distance_cam * cos(radAngleX) * sin(radAngleY);
        camera.position.y = distance_cam * sin(radAngleX);
        camera.position.z = distance_cam * cos(radAngleX) * cos(radAngleY);

        //DisableCursor();//pour pas avoir le curseur qui sort de l'ecran
        ShowCursor();//pour voir le curseur
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

        //on bouge aussi la camera de lumière
        lightCam.position = Vector3Scale(lightDir, -15.0f);
        lightCam.target = Vector3Zero();
        lightColor = GetSunColor(timeOfDay);
        lightColorNormalized = ColorNormalize(lightColor);
        SetShaderValue(shadowShader, lightColLoc, &lightColorNormalized, SHADER_UNIFORM_VEC4);
        SetShaderValue(shadowShader, lightDirLoc, &lightDir, SHADER_UNIFORM_VEC3);
        if (animIndex != 0){
            ModelAnimation anim = modelAnimations[animIndex];
            animCurrentFrame = (animCurrentFrame + 1)%anim.frameCount;
            UpdateModelAnimation(model_oiseau, anim, animCurrentFrame);
        }
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
            //update_oiseaux(oiseaux, GetFrameTime(), espacement, vitesse);
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

        update_oiseaux(oiseaux, GetFrameTime(), espacement, vitesse, maxDistanceFromCenter);
        BeginMode3D(camera);
            draw_scene(camera, oiseaux, espacement, vitesse, model_sol);
            //DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 1.0f, 1.0f, 1.0f, RED);
            DrawGrid(20, 1.0f);

        EndMode3D();
        //BeginShaderMode(shader);

        //update la lumière
        //UpdateLightValues(shader, directionalLight);

        //EndShaderMode();
        EndMode3D();
        // Affichage de l'interface utilisateur
        DrawText(" d'objets 3D - Utilisez la souris pour naviguer", 10, 10, 20, DARKGRAY);
        DrawText("Maintenez le clic droit pour tourner la scène", 10, 25, 20, DARKGRAY);
        DrawFPS(10, 40);

        /*
        l'ui pour controler les paramètres que pour linux
        */
        #if PLATFORME_LINUX
            //Pour changer la direction de la lumière
            GuiSliderBar((Rectangle){ 100, 100, 200, 20 }, "Time of Day", TextFormat("%.0f:00", timeOfDay), &timeOfDay, 0.0f, 24.0f);
            //pour changer la vitessse
            GuiSliderBar((Rectangle){ 100, 130, 200, 20 }, "Vitesse", TextFormat("%.2f", vitesse), &vitesse, 0.0f, 10.0f);
            //pour changer l'espacement
            GuiSliderBar((Rectangle){ 100, 160, 200, 20 }, "Espacement", TextFormat("%.2f", espacement), &espacement, 0.0f, 10.0f);
            //pour changer la distance max
            GuiSliderBar((Rectangle){ 100, 190, 200, 20 }, "Distance max", TextFormat("%.2f", maxDistanceFromCenter), &maxDistanceFromCenter, 0.0f, 20.0f);
        #endif
        // Affichage de l'heure
        DrawText(TextFormat("Time: %.0f:00", timeOfDay), 310, 10, 20, DARKGRAY);
        EndDrawing();
    }

    UnloadShader(shader);
    UnloadShader(shadowShader);
    // Désallocation des ressources
    UnloadModel(model_oiseau);
    UnloadModel(model_sol);
    CloseWindow();

    return 0;
}