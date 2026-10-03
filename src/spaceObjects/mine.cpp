#include "main.h"
#include "mine.h"
#include "playerInfo.h"
#include "particleEffect.h"
#include "explosionEffect.h"
#include "pathPlanner.h"
#include "random.h"
#include "multiplayer_server.h"

#include "scriptInterface.h"

#include "i18n.h"

/// A Mine is an explosive weapon that detonates and deals kinetic damage when a SpaceObject collides with its trigger range.
/// Mines can be owned by factions but are triggered by SpaceObjects of any faction can trigger them.
/// Mines can be launched from a SpaceShip's weapon tube or added by a GM or scenario script.
/// When launched from a SpaceShip, the mine has an eject timeout, during which its trigger range is inactive.
/// In 3D views, mines are represented by a particle effect at the center of its trigger range.
/// To create objects with more complex collision mechanics, use an Artifact.
/// Example: mine = Mine():setPosition(1000,1000):onDestruction(this_mine, instigator) print("Tripped a mine!") end)
REGISTER_SCRIPT_SUBCLASS(Mine, SpaceObject)
{
  /// Returns this Mine owner's SpaceObject.
  /// Works only on the server; mine ownership isn't replicated to clients.
  /// Example: mine:getOwner()
  REGISTER_SCRIPT_CLASS_FUNCTION(Mine, getOwner);
  /// Defines a function to call when this Mine is destroyed.
  /// Passes the mine object and the object of the mine's owner/instigator (or nil if there isn't one) to the function.
  /// Example: mine:onDestruction(function(this_mine, instigator) print("Tripped a mine!") end)
  REGISTER_SCRIPT_CLASS_FUNCTION(Mine, onDestruction);
}

REGISTER_MULTIPLAYER_CLASS(Mine, "Mine");
Mine::Mine()
: SpaceObjectWithSize(0, "Mine"), data(MissileWeaponData::getDataFor(MW_Mine))
{
    blink_offset = random(0, 10000);
    setSize(1);
    ensureIsInAvoidList(this);
}

Mine::~Mine()
{
}

void Mine::setSize(float scale)
{
    size = scale;
    setRadarSignatureInfo(0.f, 0.05f * scale, 0.f);
    setRadius(getTriggerRadius());
    if (scale <= 0.75f)
    {
        model_name = "mine_small";
    }
    else if (scale >= 1.5f)
    {
        model_name = "mine_big";
    }
    else
    {
        model_name = "mine_medium";
    }
    model_info.scale = glm::vec3(scale);
    updateModel();
}

void Mine::updateModel()
{
    bool is_lit;
    if (triggered) 
    {
        // pulse very rapidly
        is_lit = fmodf(engine->getElapsedTime() + blink_offset, 0.1f) < 0.05f;
    }
    else
    {
        // pulse depending on size, large mines blink slower
        auto size = getSize();
        is_lit = fmodf(engine->getElapsedTime() + blink_offset, size) < size * 0.25f;
    }
    model_info.setData(model_name+(is_lit?"_lit":"_unlit"));
}

static const float MINE_LONG_RANGE_MIN_RADAR_SIZE = 10.f;

void Mine::drawOnRadar(sp::RenderTarget& renderer, glm::vec2 position, float scale, float rotation, bool long_range)
{
    float size = getRadius() * scale * 1.6f;
    if (long_range)
    {
        size = std::fmaxf(size, MINE_LONG_RANGE_MIN_RADAR_SIZE);
    }
    renderer.drawSprite("radar/mine.png", position, size);
}

void Mine::drawOnGMRadar(sp::RenderTarget& renderer, glm::vec2 position, float scale, float rotation, bool long_range)
{
    renderer.drawCircleOutline(position, getTriggerRadius() * scale, 3.0, triggered ? glm::u8vec4(255, 0, 0, 128) : glm::u8vec4(255, 255, 255, 128));
    
    renderer.drawCircleOutline(position, getBlastRadius() * scale, 1.0, triggered ? glm::u8vec4(127, 0, 0, 128) : glm::u8vec4(255, 0, 0, 128));
}

void Mine::update(float delta)
{
    if (getRadius() != getTriggerRadius())
    {
        setSize(size);
    }

    if (particleTimeout > 0)
    {
        particleTimeout -= delta;
    }else{
        float size = getSize();
        float distance = size * 2;
        glm::vec3 pos = glm::vec3(getPosition().x, getPosition().y, 0);
        ParticleEngine::spawn(pos, pos + glm::vec3(random(-100, 100) * distance, random(-100, 100) * distance, random(-100, 100) * distance), glm::vec3(1, 1, 1), triggered ? glm::vec3(1, 0, 0) : glm::vec3(0, 0, 1), 30 * size * 2, 0, triggered ? 2.f : 10.f);
        particleTimeout = triggered ? 0.01f : 0.4f;
    }

    if (ejectTimeout > 0.0f)
    {
        ejectTimeout -= delta;
        setVelocity(vec2FromAngle(getRotation()) * (data.speed / getSize()));
    }else{
        setVelocity(glm::vec2(0, 0));
    }
    updateModel();
    if (!triggered)
        return;
    triggerTimeout += delta;

    // larger mines take longer to trigger, smaller mines trigger faster
    if (triggerTimeout >= getSize())
    {
        explode();
    }
}

void Mine::collide(Collisionable* target, float force)
{
    if (!game_server || triggered || ejectTimeout > 0.0f)
        return;
    P<SpaceObject> hitObject = P<Collisionable>(target);
    if (!hitObject || !hitObject->canBeTargetedBy(nullptr))
        return;

    triggered = true;
    particleTimeout = 0;
}

void Mine::eject()
{
    // eject timing scale:
    // - half of standard lifetime is always used as base (even the smallest mines take this long to arm)
    // - other half is multiplied by the size
    // -> small mines will arm in 75% the standard time, large mines will arm in 150% the standard time
    ejectTimeout = data.lifetime * (1.f + getSize()) * 0.5f;
}

void Mine::explode()
{
    DamageInfo info;
    if (owner)
        info = DamageInfo(owner, DT_Kinetic, getPosition());
    else
        info = DamageInfo(this, DT_Kinetic, getPosition());

    float blast_range = getBlastRadius();
    SpaceObject::damageArea(getPosition(), blast_range, getDamageEdge(), getDamageCenter(), info, blast_range / 2.0f);

    P<ExplosionEffect> e = new ExplosionEffect();
    e->setSize(blast_range);
    e->setPosition(getPosition());
    e->setOnRadar(true);
    e->setRadarSignatureInfo(0.0, 0.0, 0.2f * getSize());

    if (on_destruction.isSet())
    {
        if (info.instigator)
        {
            on_destruction.call<void>(P<Mine>(this), P<SpaceObject>(info.instigator));
        }else{
            on_destruction.call<void>(P<Mine>(this));
        }
    }
    destroy();
}

void Mine::onDestruction(ScriptSimpleCallback callback)
{
    this->on_destruction = callback;
}

P<SpaceObject> Mine::getOwner()
{
    if (game_server)
    {
        return owner;
    }

    LOG(ERROR) << "Mine::getOwner(): owner not replicated to clients.";
    return nullptr;
}

std::unordered_map<string, string> Mine::getGMInfo()
{
    std::unordered_map<string, string> ret;

    if (owner)
    {
        ret[trMark("gm_info", "Owner")] = owner->getCallSign();
    }

    ret[trMark("gm_info", "Faction")] = getLocaleFaction();

    return ret;
}
