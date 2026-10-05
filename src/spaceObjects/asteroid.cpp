#include <graphics/opengl.h>
#include <glm/gtc/type_ptr.hpp>
#include "asteroid.h"
#include "explosionEffect.h"
#include "main.h"
#include "random.h"

#include "scriptInterface.h"
#include "glObjects.h"
#include "shaderRegistry.h"
#include "textureManager.h"

#include <glm/ext/matrix_transform.hpp>

REGISTER_SCRIPT_SUBCLASS_NO_CREATE(AbstractAsteroid, SpaceObjectWithSize)
{
}

/// An Asteroid is an inert piece of space terrain.
/// Upon collision with another SpaceObject, it deals damage and is destroyed.
/// It has a default rotation speed, random z-offset, and model, and AI behaviors attempt to avoid hitting them.
/// To create a customizable object with more complex actions upon collisions, use an Artifact or SupplyDrop.
/// For a purely decorative asteroid positioned outside of the movement plane, use a VisualAsteroid.
/// Example: asteroid = Asteroid():setSize(150):setPosition(1000,2000)
REGISTER_SCRIPT_SUBCLASS(Asteroid, AbstractAsteroid)
{
}

AbstractAsteroid::AbstractAsteroid(string multiplayer_name)
    : SpaceObjectWithSize(random(110, 130), multiplayer_name)
{
    setRotation(random(0, 360));
    rotation_speed = random(0.1f, 0.8f);
    z = random(-50, 50);
    model_number = irandom(1, 10);

    registerMemberReplication(&z);
    registerMemberReplication(&rotation_speed);
    registerMemberReplication(&model_number);

    // if the subclasses ever get custom implementations for ANYTHING relating to setSize/radius/etc., this needs to be moved into their constructors!
    setSize(size);
}

void AbstractAsteroid::setSize(float size)
{
    SpaceObjectWithSize::setSize(size);

    // signature based on actual size (technically x2 radius would be a lot more than x2 gravity, no?)
    setRadarSignatureInfo(0.05f * size / 100.f, 0, 0);
}

void AbstractAsteroid::draw3D()
{
    auto model_matrix = getModelMatrix();
    ShaderRegistry::ScopedShader shader(ShaderRegistry::Shaders::ObjectSpecular);

    glUniformMatrix4fv(shader.get().uniform(ShaderRegistry::Uniforms::Model), 1, GL_FALSE, glm::value_ptr(model_matrix));

    textureManager.getTexture("Astroid_" + string(model_number) + "_d.png")->bind();

    glActiveTexture(GL_TEXTURE0 + ShaderRegistry::textureIndex(ShaderRegistry::Textures::SpecularMap));
    textureManager.getTexture("Astroid_" + string(model_number) + "_s.png")->bind();

    Mesh* m = Mesh::getMesh("Astroid_" + string(model_number) + ".model");

    gl::ScopedVertexAttribArray positions(shader.get().attribute(ShaderRegistry::Attributes::Position));
    gl::ScopedVertexAttribArray texcoords(shader.get().attribute(ShaderRegistry::Attributes::Texcoords));
    gl::ScopedVertexAttribArray normals(shader.get().attribute(ShaderRegistry::Attributes::Normal));

    ShaderRegistry::setupLights(shader.get(), model_matrix);
    m->render(positions.get(), texcoords.get(), normals.get());


    glActiveTexture(GL_TEXTURE0);
}

glm::mat4 AbstractAsteroid::getModelMatrix() const
{
    auto asteroid_matrix = glm::translate(SpaceObject::getModelMatrix(), glm::vec3(0.f, 0.f, z));
    asteroid_matrix = glm::rotate(asteroid_matrix, glm::radians(engine->getElapsedTime() * rotation_speed), glm::vec3(0.f, 0.f, 1.f));
    return glm::scale(asteroid_matrix, glm::vec3(getRadius()));
}

REGISTER_MULTIPLAYER_CLASS(Asteroid, "Asteroid");
Asteroid::Asteroid()
: AbstractAsteroid("Asteroid")
{
    setCollisionTypeStatic();   // static bodies do not collide with other static bodies
                                // currently only asteroids are static bodies
    ensureIsInAvoidList(this);
}

void Asteroid::setMovable(bool movable)
{
    is_movable = movable;
}

float Asteroid::getAvoidSize()
{
    // orig: size=110 to 130, avoid=300
    // TODO: This seems excessive - are AIs even capable of navigating asteroid fields??
    return getSize() * 1.5f + 120;
}

void Asteroid::drawOnRadar(sp::RenderTarget& renderer, glm::vec2 position, float scale, float rotation, bool long_range)
{
    renderer.drawSprite("radar/blip.png", position, std::max(6.0f, (getRadius() * 2.0f) * scale), glm::u8vec4(255, 200, 100, 255));
}

void Asteroid::collide(Collisionable* target, float force)
{
    if (!isServer())
        return;
    P<SpaceObject> hit_object = P<Collisionable>(target);
    if (!hit_object || !hit_object->canBeTargetedBy(nullptr))
        return;

    DamageInfo info(this, DT_Kinetic, getPosition());
    hit_object->takeDamage(0.35f * getRadius(), info);

    P<ExplosionEffect> e = new ExplosionEffect();
    e->setSize(getRadius());
    e->setPosition(getPosition());
    e->setRadarSignatureInfo(0.f, 0.1f, 0.2f);
    destroy();
}

/// A VisualAsteroid is an inert piece of space terrain positioned above or below the movement plane.
/// For an asteroid that ships might collide with, use an Asteroid.
/// Example: vasteroid = VisualAsteroid():setSize(150):setPosition(1000,2000)
REGISTER_SCRIPT_SUBCLASS(VisualAsteroid, AbstractAsteroid)
{
}

REGISTER_MULTIPLAYER_CLASS(VisualAsteroid, "VisualAsteroid");
VisualAsteroid::VisualAsteroid()
: AbstractAsteroid("VisualAsteroid")
{
    setZ(z < 0 ? (-150 - z*2 - size) : (150 + z*2 + size));
}

void VisualAsteroid::setSize(float size)
{
    auto previous_offset = z + getSize() * (z < 0 ? 1.f : -1.f);
    setSizeAndZ(size, previous_offset + (z < 0 ? -size : size));
}

void VisualAsteroid::setSizeAndZ(float size, float z)
{
    setZ(z);
    AbstractAsteroid::setSize(size);
}

void VisualAsteroid::drawOnRadar(sp::RenderTarget& renderer, glm::vec2 position, float scale, float rotation, bool long_range)
{
    renderer.drawSprite("radar/blip.png", position, getRadius() * 2.0f * scale, glm::u8vec4(200, 150, 50, 50));
}
