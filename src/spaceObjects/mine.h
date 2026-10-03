#ifndef MINE_H
#define MINE_H

#include "spaceObjectWithSize.h"
#include "pathPlanner.h"

class Mine : public SpaceObjectWithSize, public IAvoidableSpaceObject
{
    constexpr static float blastRange = 1000.0f;
    constexpr static float trigger_range = 600.0f;
    constexpr static float triggerDelay = 1.0f;
    constexpr static float damageAtCenter = 160.0f;
    constexpr static float damageAtEdge = 30.0f;

    ScriptSimpleCallback on_destruction;

public:
    P<SpaceObject> owner;
    bool triggered = false;   //Only valid on server.
    float triggerTimeout = 0; //Only valid on server.
    float ejectTimeout = 0;   //Only valid on server.
    float particleTimeout = 0;
    float blink_offset;

    Mine();
    virtual ~Mine();

    //virtual void draw3D() override;
    virtual void drawOnRadar(sp::RenderTarget& renderer, glm::vec2 position, float scale, float rotation, bool long_range) override;
    virtual void drawOnGMRadar(sp::RenderTarget& renderer, glm::vec2 position, float scale, float rotation, bool long_range) override;
    virtual void update(float delta) override;
    virtual void updateModel();

    virtual void collide(Collisionable* target, float force) override;
    virtual float getAvoidSize() override {
        return getBlastRadius() * 1.2f;
    }
    virtual float getBlastRadius() {
        return getSize() * blastRange;
    }
    virtual float getTriggerRadius() {
        return getSize() * trigger_range;
    }
    virtual float getDamageCenter() {
        return getSize() * damageAtCenter;
    }
    virtual float getDamageEdge() {
        return getSize() * damageAtEdge;
    }
    virtual float getReasonableMaxValue() override {
        return 3;
    }
    virtual void setSize(float size) override;
    void eject();
    void explode();
    void onDestruction(ScriptSimpleCallback callback);

    P<SpaceObject> getOwner();
    virtual std::unordered_map<string, string> getGMInfo() override;
    virtual string getExportLine() override { string s = "Mine():setPosition(" + string(getPosition().x, 0) + ", " + string(getPosition().y, 0) + ")"; if (getSize() != 1.f) { s += ":setSize(" + string(getSize(), 2) + ")"; } return s; }

private:
    const MissileWeaponData& data;
    string model_name = "";
};

#endif//MINE_H
