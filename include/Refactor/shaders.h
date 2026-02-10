//
// Created by forna on 10.02.2026.
//

#ifndef SHADERS_H
#define SHADERS_H
#include <string>
inline const char *ModelVertex = R"(
      #version 330 core
      precision mediump float;

      layout(location=0) in vec3 aPos;
     layout(location=1) in vec3 aNormal;
     layout(location=2) in vec2 aTex;

     // instancing mat4 : locations 5..8
     layout(location=5) in vec4 iM0;
     layout(location=6) in vec4 iM1;
     layout(location=7) in vec4 iM2;
     layout(location=8) in vec4 iM3;

     uniform mat4 uView;
     uniform mat4 uProj;

     out vec3 vWorldPos;
     out vec3 vWorldNormal;
     out vec2 vUV;

     void main() {
         mat4 M = mat4(iM0, iM1, iM2, iM3);

         vec4 worldPos = M * vec4(aPos, 1.0);
         vWorldPos = worldPos.xyz;

         mat3 N = mat3(transpose(inverse(M)));
         vWorldNormal = normalize(N * aNormal);

         vUV = aTex;
         gl_Position = uProj * uView * worldPos;
     }
)";
inline const char *ModelFragment = R"(
     #version 330 core
     precision mediump float;

     layout(location=0) out vec4 gPosition;
     layout(location=1) out vec4 gNormal;
     layout(location=2) out vec4 gAlbedoSpec;

     in vec3 vWorldPos;
     in vec3 vWorldNormal;
     in vec2 vUV;

     uniform sampler2D texture_diffuse1;

     void main() {
         gPosition = vec4(vWorldPos, 1.0);
         gNormal = vec4(normalize(vWorldNormal) * 0.5 + 0.5, 1.0);

         vec3 albedo = texture(texture_diffuse1, vUV).rgb;

         //fallback si texture vide
         if(length(albedo) < 0.001) albedo = vec3(0.8);

        gAlbedoSpec = vec4(albedo, 1.0);
     }
)";

const char *PostVertex = R"(
     #version 330 core
     precision mediump float;

     layout(location = 0) in vec2 aPos;
     layout(location = 1) in vec2 aUV;

     out vec2 vUV;

     void main() {
           vUV = aUV;
           gl_Position = vec4(aPos, 0.0, 1.0);
     }
)";

const char *PostFragment = R"(
    #version 330 core
    precision mediump float;

    in vec2 vUV;
    out vec4 FragColor;

    uniform sampler2D uScene;
    uniform int uEffect;
    uniform vec2 uTexelSize;
    uniform float uTime;
    uniform float uBloomThreshold;
    uniform float uBloomIntensity;

    // Fonction pour convertir en grayscale
    float luminance(vec3 color) {
        return dot(color, vec3(0.299, 0.587, 0.114));
    }

    // Fonction de convolution 3x3
    vec3 convolution3x3(sampler2D tex, vec2 uv, vec2 texelSize) {
        vec3 sum = vec3(0.0);

        // Kernel d'edge detection par défaut
        float kernel[9] = float[](
            -1.0, -1.0, -1.0,
            -1.0,  8.0, -1.0,
            -1.0, -1.0, -1.0
        );

        if (uEffect == 5) { // Flou
            kernel = float[](
                1.0/16.0, 2.0/16.0, 1.0/16.0,
                2.0/16.0, 4.0/16.0, 2.0/16.0,
                1.0/16.0, 2.0/16.0, 1.0/16.0
            );
        } else if (uEffect == 6) { // Renforcement
            kernel = float[](
                0.0, -1.0, 0.0,
                -1.0, 5.0, -1.0,
                0.0, -1.0, 0.0
            );
        }

        int index = 0;
        for (int y = -1; y <= 1; y++) {
            for (int x = -1; x <= 1; x++) {
                vec2 offset = vec2(float(x), float(y)) * texelSize;
                vec3 color = texture(tex, uv + offset).rgb;
                sum += color * kernel[index];
                index++;
            }
        }
        return sum;
    }

    void main() {
        vec3 color = texture(uScene, vUV).rgb;
        vec3 result = color;

        if (uEffect == 1) { // Inversion
            result = vec3(1.0) - color;
        }
        else if (uEffect == 2) { // Grayscale
            float gray = luminance(color);
            result = vec3(gray);
        }
        else if (uEffect == 3) { // Sepia
            float gray = luminance(color);
            result = vec3(gray * vec3(1.2, 1.0, 0.8));
        }
        else if (uEffect == 4) { // Edge Detection
            result = convolution3x3(uScene, vUV, uTexelSize);
            result = clamp(result, 0.0, 1.0);
        }
        else if (uEffect == 5) { // Flou
            result = convolution3x3(uScene, vUV, uTexelSize);
        }
        else if (uEffect == 6) { // Renforcement
            result = convolution3x3(uScene, vUV, uTexelSize);
            result = clamp(result, 0.0, 1.0);
        }
        else if (uEffect == 7) { // Emboss
            // Kernel emboss simplifié
            result = vec3(0.5) + (color - texture(uScene, vUV - vec2(1.0, 1.0) * uTexelSize).rgb);
        }
        else if (uEffect == 8) { // Sobel simplifié
            vec3 gx = vec3(0.0);
            vec3 gy = vec3(0.0);

            float sobelX[9] = float[](
                -1.0, 0.0, 1.0,
                -2.0, 0.0, 2.0,
                -1.0, 0.0, 1.0
            );

            float sobelY[9] = float[](
                -1.0, -2.0, -1.0,
                0.0,  0.0,  0.0,
                1.0,  2.0,  1.0
            );

            int index = 0;
            for (int y = -1; y <= 1; y++) {
                for (int x = -1; x <= 1; x++) {
                    vec2 offset = vec2(float(x), float(y)) * uTexelSize;
                    vec3 texColor = texture(uScene, vUV + offset).rgb;
                    gx += texColor * sobelX[index];
                    gy += texColor * sobelY[index];
                    index++;
                }
            }

            result = sqrt(gx * gx + gy * gy);
            result = clamp(result, 0.0, 1.0);
        }
        else if (uEffect == 9) { // Gaussian Blur simplifié
            vec3 sum = vec3(0.0);
            float weight = 1.0 / 9.0;

            for (int y = -1; y <= 1; y++) {
                for (int x = -1; x <= 1; x++) {
                    vec2 offset = vec2(float(x), float(y)) * uTexelSize;
                    sum += texture(uScene, vUV + offset).rgb * weight;
                }
            }
            result = sum;
        }
        else if (uEffect == 11) { // Vision Nocturne
            float gray = luminance(color);
            result = vec3(0.1, 0.8, 0.1) * gray;

            // Ajouter du bruit
            float noise = fract(sin(dot(vUV, vec2(12.9898, 78.233))) * 43758.5453);
            result += noise * 0.05;

            // Ajouter des scanlines
            if (mod(gl_FragCoord.y, 3.0) < 1.0) {
                result *= 0.9;
            }
        }

        FragColor = vec4(result, 1.0);
    }
)";

// 2) shaders skybox
const std::string VertexSkyBox = R"(
    #version 300 es
    precision mediump float;
    layout (location = 0) in vec3 aPos;
    out vec3 TexCoords;

    uniform mat4 uView;
    uniform mat4 uProj;

    void main() {
        TexCoords = aPos;

        // enlever la translation de la view (skybox centrée)
        mat4 viewNoTranslation = mat4(mat3(uView));
        vec4 pos = uProj * viewNoTranslation * vec4(aPos, 1.0);

        // profondeur à 1.0
        gl_Position = pos.xyww;
    }
)";

const std::string FragmentSkybox = R"(
    #version 300 es
    precision mediump float;
    precision mediump samplerCube;
    in vec3 TexCoords;
    out vec4 FragColor;

    uniform samplerCube uSkybox;

    void main() {
        FragColor = texture(uSkybox, TexCoords);
    }
)";

// Shader d'extraction des zones lumineuses
const char *bloomExtractFS = R"(
        #version 330 core
        precision mediump float;

        in vec2 vUV;
        out vec4 FragColor;

        uniform sampler2D uScene;
        uniform float uThreshold;

        float luminance(vec3 color) {
            return dot(color, vec3(0.299, 0.587, 0.114));
        }

        void main() {
            vec3 color = texture(uScene, vUV).rgb;
            float brightness = luminance(color);

            if (brightness > uThreshold) {
                FragColor = vec4(color, 1.0);
            } else {
                FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            }
        }
    )";

const char *bloomVertexShader = R"(
        #version 330 core
        precision mediump float;

        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aUV;

        out vec2 vUV;

        void main() {
            vUV = aUV;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

// Shader de flou gaussien - CORRIGÉ
const char *bloomBlurFS = R"(
        #version 330 core
        precision mediump float;

        in vec2 vUV;
        out vec4 FragColor;

        uniform sampler2D uImage;
        uniform bool uHorizontal;
        uniform vec2 uTexelSize;

        void main() {
            vec2 texelSize = uTexelSize;
            vec3 result = vec3(0.0);

            // Noyau gaussien 9x1
            float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

            result = texture(uImage, vUV).rgb * weight[0];

            if (uHorizontal) {
                for (int i = 1; i < 5; i++) {
                    result += texture(uImage, vUV + vec2(texelSize.x * float(i), 0.0)).rgb * weight[i];
                    result += texture(uImage, vUV - vec2(texelSize.x * float(i), 0.0)).rgb * weight[i];
                }
            } else {
                for (int i = 1; i < 5; i++) {
                    result += texture(uImage, vUV + vec2(0.0, texelSize.y * float(i))).rgb * weight[i];
                    result += texture(uImage, vUV - vec2(0.0, texelSize.y * float(i))).rgb * weight[i];
                }
            }

            FragColor = vec4(result, 1.0);
        }
    )";

// Shader de combinaison bloom - SIMPLIFIÉ POUR ÉVITER LES ERREURS
const char *bloomCombineFS = R"(
        #version 330 core
        precision mediump float;

        in vec2 vUV;
        out vec4 FragColor;

        uniform sampler2D uScene;
        uniform sampler2D uBloomBlur;
        uniform float uBloomIntensity;

        void main() {
            vec3 scene = texture(uScene, vUV).rgb;
            vec3 bloom = texture(uBloomBlur, vUV).rgb;

            // Combinaison additive
            vec3 result = scene + bloom * uBloomIntensity;

            FragColor = vec4(result, 1.0);
        }
    )";

const std::string ModelCube = R"(
    #version 330 core

    layout(location=0) in vec3 aPos;
    layout(location=1) in vec3 aNormal;
    layout(location=2) in vec2 aUV;
    layout(location=3) in vec3 aTangent;

    // instancing
    layout(location=4) in vec4 iM0;
    layout(location=5) in vec4 iM1;
    layout(location=6) in vec4 iM2;
    layout(location=7) in vec4 iM3;

    uniform mat4 uView;
    uniform mat4 uProj;

    out vec2 vUV;
    out vec3 vPosWS;
    out mat3 vTBN;
    out vec3 Normal;

    void main()
    {
        mat4 M = mat4(iM0, iM1, iM2, iM3);
        mat3 normalMatrices= mat3(transpose(inverse(M)));
        Normal=normalize(normalMatrices * aNormal);
        vec3 N = normalize(normalMatrices * aNormal);
        vec3 T = normalize(normalMatrices * aTangent);
        T = normalize(T - dot(T, N) * N);
        vec3 B = cross(N, T);

        vTBN = mat3(T, B, N);
        vUV = aUV;
        vec4 worldPos = M * vec4(aPos, 1.0);
        vPosWS = worldPos.xyz;

        gl_Position = uProj * uView * worldPos;
    }
)";

const std::string ModulFragment = R"(
#version 330 core
layout(location=0) out vec4 gPosition;
layout(location=1) out vec4 gNormal;
layout(location=2) out vec4 gAlbedoSpec;


in vec2 vUV;
in vec3 vPosWS;
in mat3 vTBN;
in vec3 Normal;

uniform sampler2D uDiffuse;
uniform sampler2D uNormalMap;
uniform bool useNormal;

void main(){

    gPosition=vec4(vPosWS, 1.0);

    // tangent -> world
    vec3 N = normalize(Normal);

    if(useNormal){
        // normal map (tangent space)
        vec3 nTS = texture(uNormalMap, vUV).rgb;
        nTS = normalize(nTS * 2.0 - 1.0); // [0,1] -> [-1,1]
        N=normalize(vTBN * nTS);
    }
    gNormal=vec4(N*0.5+0.5, 1.0);

    vec4 diffuseTex = texture(uDiffuse, vUV);
    vec3 albedo = diffuseTex.rgb;
    float specular = 0.6; // Force spéculaire constante pour test

    if(length(albedo) < 0.01) {
        albedo = vec3(0.8, 0.6, 0.4); // Couleur de brique par défaut
    }

    gAlbedoSpec = vec4(albedo, specular);
}
)";
const std::string drv = R"(#version 330 core
        layout(location=0) in vec2 aPos;
        layout(location=1) in vec2 aTexCoord;
        out vec2 TexCoord;
void main(){
    TexCoord= aTexCoord;
    gl_Position= vec4(aPos.x, aPos.y, 0.0, 1.0);
}

)";
const std::string drf = R"(#version 330 core
layout(location=0) out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

uniform vec3 viewPos;
uniform float specularPow;

uniform samplerCube uSkybox;
uniform float far;
uniform mat4 invProj;
uniform mat4 invViewRot;

vec3 GetSkyDir(vec2 uv, vec3 viewPosition)
    {
        vec2 ndc = uv * 2.0 - 1.0;              // [-1,1]
        vec4 clip = vec4(ndc, 1.0, 1.0);

        vec4 viewRay = invProj * clip;
        viewRay = vec4(viewRay.xy, -1.0, 0.0);              // direction en view-space

        vec3 dirVS = normalize(viewRay.xyz);
        vec3 dirWS = normalize((invViewRot * vec4(dirVS, 0.0)).xyz);
        return dirWS;
    }

struct PointLight{
    vec3 position;
    vec3 color;
    float constant;
    float linear;
    float quadratic;
};
uniform int pointLightCount;
uniform PointLight pointLights[1];

uniform int debugMode;

void main(){

    // Mode debug: 0=normal, 1=positions, 2=normales, 3=albedo, 4=UV
    if(debugMode == 1) {
        // Afficher les positions (normalisées pour la visualisation)
        vec3 pos = texture(gPosition, TexCoord).rgb;
        FragColor = vec4(normalize(pos) * 0.5 + 0.5, 1.0);
        return;
    }
    else if(debugMode == 2) {
        // Afficher les normales (déjà en [0,1])
        FragColor = texture(gNormal, TexCoord);
        return;
    }
    else if(debugMode == 3) {
        // Afficher l'albedo (couleur de la texture)
        FragColor = texture(gAlbedoSpec, TexCoord);
        return;
    }
    else if(debugMode == 4) {
        // Afficher les coordonnées UV
        FragColor = vec4(TexCoord, 0.0, 1.0);
        return;
    }

      // Lire les données du G-buffer
    vec4 positionData = texture(gPosition, TexCoord);
    vec3 fragPos = positionData.rgb;

    // SI c'est le fond (position ~ 0), afficher la skybox
    if(length(fragPos) < 0.001) {
        // Utiliser viewPos pour un calcul correct
        vec3 rayDir = GetSkyDir(TexCoord, viewPos);
        vec3 skyColor = texture(uSkybox, rayDir).rgb;
        FragColor = vec4(skyColor, 1.0);

        // IMPORTANT: Définir une profondeur lointaine
        gl_FragDepth = 1.0; // Presque 1.0
        return;
    }

    // Sinon, faire l'éclairage normal...
    vec3 normal = normalize(texture(gNormal, TexCoord).rgb * 2.0 - 1.0);
    vec3 albedo = texture(gAlbedoSpec, TexCoord).rgb;
    float specularStrength = texture(gAlbedoSpec, TexCoord).a;

    // Éclairage (votre code existant)...
    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 lighting = albedo * 0.1;

    for(int i = 0; i < pointLightCount; i++){
        vec3 lightDir = normalize(pointLights[i].position - fragPos);
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = diff * albedo * pointLights[i].color;

        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), specularPow);
        vec3 specular = specularStrength * spec * pointLights[i].color;

        float distance = length(pointLights[i].position - fragPos);
        float attenuation = 1.0 / (pointLights[i].constant +
                          pointLights[i].linear * distance +
                          pointLights[i].quadratic * (distance * distance));

        lighting += (diffuse + specular) * attenuation;
    }

    FragColor = vec4(lighting, 1.0);
    gl_FragDepth = viewPos.z;
})";

// SHADER SHADOW CORRECT - FRAGMENT SHADER DOIT ÉCRIRE LA PROFONDEUR
const std::string shadowVS = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;

        uniform mat4 uLightSpaceMatrix;
        uniform mat4 uModel;

        void main() {
            gl_Position = uLightSpaceMatrix * uModel * vec4(aPos, 1.0);
        }
    )";

const std::string shadowFS = R"(
        #version 330 core

        void main() {
            // Fragment shader vide - la profondeur est écrite automatiquement
            // gl_FragDepth est automatiquement rempli avec gl_Position.z
        }
    )";

// Créer les shaders SSAO - VERSION SIMPLIFIÉE
const std::string ssaoVS = R"(
            #version 330 core
            layout(location = 0) in vec2 aPos;
            layout(location = 1) in vec2 aTexCoord;

            out vec2 TexCoord;

            void main() {
                TexCoord = aTexCoord;
                gl_Position = vec4(aPos, 0.0, 1.0);
            }
        )";

const std::string ssaoFS = R"(
            #version 330 core
            out float FragColor;

            in vec2 TexCoord;

            uniform sampler2D gPosition;
            uniform sampler2D gNormal;
            uniform sampler2D texNoise;

            uniform mat4 projection;

            const int kernelSize = 16;
            uniform vec3 samples[16];
            uniform float radius = 0.5;
            uniform float bias = 0.025;

            // Tile noise texture over screen
            const vec2 noiseScale = vec2(800.0/4.0, 600.0/4.0);

            void main() {
                // Get input for SSAO algorithm
                vec3 fragPos = texture(gPosition, TexCoord).rgb;
                vec3 normal = normalize(texture(gNormal, TexCoord).rgb * 2.0 - 1.0);
                vec3 randomVec = normalize(texture(texNoise, TexCoord * noiseScale).xyz);

                // Create TBN matrix
                vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
                vec3 bitangent = cross(normal, tangent);
                mat3 TBN = mat3(tangent, bitangent, normal);

                // Calculate occlusion
                float occlusion = 0.0;
                for(int i = 0; i < kernelSize; ++i) {
                    // Get sample position
                    vec3 samplePos = TBN * samples[i];
                    samplePos = fragPos + samplePos * radius;

                    // Project sample position
                    vec4 offset = vec4(samplePos, 1.0);
                    offset = projection * offset;
                    offset.xyz /= offset.w;
                    offset.xyz = offset.xyz * 0.5 + 0.5;

                    // Get sample depth
                    float sampleDepth = texture(gPosition, offset.xy).z;

                    // Range check and accumulate
                    float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
                    occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
                }

                occlusion = 1.0 - (occlusion / float(kernelSize));
                FragColor = occlusion;
            }
        )";

const std::string ssaoBlurFS = R"(
            #version 330 core
            out float FragColor;

            in vec2 TexCoord;

            uniform sampler2D ssaoInput;

            void main() {
                vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
                float result = 0.0;
                for(int x = -2; x <= 2; ++x) {
                    for(int y = -2; y <= 2; ++y) {
                        vec2 offset = vec2(float(x), float(y)) * texelSize;
                        result += texture(ssaoInput, TexCoord + offset).r;
                    }
                }
                FragColor = result / 25.0;
            }
        )";

// Shader simplifié pour debug
const std::string debugVS = R"(
        #version 330 core
        layout(location=0) in vec2 aPos;
        layout(location=1) in vec2 aTexCoord;
        out vec2 TexCoord;
        void main() {
            TexCoord = aTexCoord;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

const std::string debugFS = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;

        uniform sampler2D gAlbedoSpec;

        void main() {


            // Juste afficher l'albedo
            vec4 albedo = texture(gAlbedoSpec, TexCoord);

            if(albedo.a < 0.1) {
                // Fond
                FragColor = vec4(0.1, 0.1, 0.15, 1.0);
            } else {
                // Multiplier pour mieux voir
                FragColor = vec4(albedo.rgb * 2.0, 1.0);
            }
        }
    )";

// Shader shadow simplifié - CORRIGÉ
const std::string shadowSimpleFS = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;

        uniform sampler2D gPosition;
        uniform sampler2D gNormal;
        uniform sampler2D gAlbedoSpec;
        uniform sampler2D shadowMap;
        uniform mat4 lightSpaceMatrix;
        uniform vec3 viewPos;
        uniform vec3 lightPos;

        float ShadowCalculation(vec4 fragPosLightSpace) {
            vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
            projCoords = projCoords * 0.5 + 0.5;

            if(projCoords.z > 1.0) return 0.0;

            float closestDepth = texture(shadowMap, projCoords.xy).r;
            float currentDepth = projCoords.z;

            float bias = 0.005;
            float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;

            return shadow;
        }

        void main() {
            vec3 fragPos = texture(gPosition, TexCoord).rgb;
            vec3 normal = normalize(texture(gNormal, TexCoord).rgb * 2.0 - 1.0);
            vec3 albedo = texture(gAlbedoSpec, TexCoord).rgb;
            float specular = texture(gAlbedoSpec, TexCoord).a;

            // Vérifier si c'est un pixel vide
            if(length(fragPos) < 0.01) {
                FragColor = vec4(0.1, 0.1, 0.15, 1.0);
                return;
            }

            // Lumière directionnelle
            vec3 lightDir = normalize(lightPos - fragPos);
            vec3 lightColor = vec3(1.0, 0.95, 0.9);

            // Ambient
            vec3 ambient = albedo * 0.1;

            // Diffuse
            float diff = max(dot(normal, lightDir), 0.0);
            vec3 diffuse = diff * albedo * lightColor;

            // Specular (Blinn-Phong)
            vec3 viewDir = normalize(viewPos - fragPos);
            vec3 halfwayDir = normalize(lightDir + viewDir);
            float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
            vec3 specularLight = specular * spec * lightColor;

            // Calcul ombre
            vec4 fragPosLightSpace = lightSpaceMatrix * vec4(fragPos, 1.0);
            float shadow = ShadowCalculation(fragPosLightSpace);

            // Résultat final avec ombres
            vec3 lighting = ambient + (1.0 - shadow) * (diffuse + specularLight);
            FragColor = vec4(lighting, 1.0);
        }
    )";

const std::string fs = R"(
            #version 330 core
            layout(location=0) in vec2 aPos;
            layout(location=1) in vec2 aTexCoord;
            out vec2 TexCoord;
            void main() {
                TexCoord = aTexCoord;
                gl_Position = vec4(aPos, 0.0, 1.0);
            }
        )";

const std::string SSAOvs = R"(
            #version 330 core
            layout(location=0) in vec2 aPos;
            layout(location=1) in vec2 aTexCoord;
            out vec2 TexCoord;
            void main() {
                TexCoord = aTexCoord;
                gl_Position = vec4(aPos, 0.0, 1.0);
            }
        )";

const std::string simpleShadowVS = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;

        uniform mat4 uLightSpaceMatrix;
        uniform mat4 uModel;

        void main() {
            gl_Position = uLightSpaceMatrix * uModel * vec4(aPos, 1.0);
        }
    )";

const std::string simpleShadowFS = R"(
        #version 330 core
        void main() {
            // Fragment shader minimal mais valide
            // gl_FragDepth sera automatiquement gl_Position.z
        }
    )";

const std::string ssaoFS2 = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;

        uniform sampler2D gPosition;
        uniform sampler2D gNormal;
        uniform sampler2D gAlbedoSpec;
        uniform sampler2D ssaoBuffer;
        uniform vec3 viewPos;

        void main() {
            vec3 fragPos = texture(gPosition, TexCoord).rgb;
            vec3 normal = normalize(texture(gNormal, TexCoord).rgb * 2.0 - 1.0);
            vec3 albedo = texture(gAlbedoSpec, TexCoord).rgb;
            float specular = texture(gAlbedoSpec, TexCoord).a;
            float ao = texture(ssaoBuffer, TexCoord).r;

            if(length(fragPos) < 0.01) {
                FragColor = vec4(0.1, 0.1, 0.15, 1.0);
                return;
            }

            // Lumière directionnelle
            vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
            vec3 lightColor = vec3(1.0, 0.95, 0.9);

            // Ambient avec occlusion SSAO
            vec3 ambient = albedo * 0.1 * ao;

            // Diffuse
            float diff = max(dot(normal, lightDir), 0.0);
            vec3 diffuse = diff * albedo * lightColor;

            // Specular (Blinn-Phong)
            vec3 viewDir = normalize(viewPos - fragPos);
            vec3 halfwayDir = normalize(lightDir + viewDir);
            float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
            vec3 specularLight = specular * spec * lightColor;

            vec3 lighting = ambient + diffuse + specularLight;
            FragColor = vec4(lighting, 1.0);
        }
    )";

const std::string SSAOvs2 = R"(
            #version 330 core
            layout(location=0) in vec2 aPos;
            layout(location=1) in vec2 aTexCoord;
            out vec2 TexCoord;
            void main() {
                TexCoord = aTexCoord;
                gl_Position = vec4(aPos, 0.0, 1.0);
            }
        )";

const std::string debugFS2 = R"(
        #version 330 core
        out vec4 FragColor;
        in vec2 TexCoord;
        uniform sampler2D depthMap;

        void main() {
            float depth = texture(depthMap, TexCoord).r;

            // Afficher en heatmap pour mieux voir
            vec3 color;

            if (depth > 0.999) {
                // Profondeur maximale = noir (pas d'objet)
                color = vec3(0.0, 0.0, 0.0);
            } else if (depth < 0.3) {
                // Très proche = bleu
                color = mix(vec3(0.0, 0.0, 1.0), vec3(0.0, 1.0, 1.0), depth/0.3);
            } else if (depth < 0.6) {
                // Moyen = cyan -> jaune
                color = mix(vec3(0.0, 1.0, 1.0), vec3(1.0, 1.0, 0.0), (depth-0.3)/0.3);
            } else {
                // Loin = jaune -> rouge
                color = mix(vec3(1.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0), (depth-0.6)/0.4);
            }

            // Augmenter le contraste
            color = pow(color, vec3(0.5));
            FragColor = vec4(color, 1.0);
        }
    )";

const std::string debugVS2 = R"(
            #version 330 core
            layout(location=0) in vec2 aPos;
            layout(location=1) in vec2 aTexCoord;
            out vec2 TexCoord;
            void main() {
                TexCoord = aTexCoord;
                gl_Position = vec4(aPos, 0.0, 1.0);
            }
        )";

const std::string debugFS3 = R"(
            #version 330 core
            out vec4 FragColor;
            in vec2 TexCoord;
            uniform sampler2D ssaoBuffer;

            void main() {
                float ao = texture(ssaoBuffer, TexCoord).r;
                FragColor = vec4(vec3(ao), 1.0);
            }
        )";

const std::string debugVS3 = R"(
                #version 330 core
                layout(location=0) in vec2 aPos;
                layout(location=1) in vec2 aTexCoord;
                out vec2 TexCoord;
                void main() {
                    TexCoord = aTexCoord;
                    gl_Position = vec4(aPos, 0.0, 1.0);
                }
            )";

#endif //SHADERS_H
