#ifndef BLACK_HOLE_H
#define BLACK_HOLE_H

#include "spaceObjectWithSize.h"

class BlackHole : public SpaceObjectWithSize
{
    float update_delta = 0.f;

public:
    BlackHole();

    virtual void update(float delta) override;

    virtual void draw3DTransparent() override;
    virtual void drawOnRadar(sp::RenderTarget& renderer, glm::vec2 position, float scale, float rotation, bool long_range) override;

    virtual bool canHideInNebula()  override { return false; }
    virtual ERadarLayer getRadarLayer() const override { return ERadarLayer::BackgroundObjects; }

    virtual void collide(Collisionable* target, float force) override;

    virtual string getExportLine() override { return "BlackHole():setPosition(" + string(getPosition().x, 0) + ", " + string(getPosition().y, 0) + "):setSize("+ string(getSize(), 0) + ")"; }

    virtual void setSize(float size) override;
    virtual float getReasonableMaxValue() override { return 50000; }
};

#endif//BLACK_HOLE_H
