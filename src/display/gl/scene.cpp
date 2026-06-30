/*
** scene.cpp
**
** This file is part of mkxp.
**
** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
**
** mkxp is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 2 of the License, or
** (at your option) any later version.
**
** mkxp is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "scene.h"
#include "sharedstate.h"

#ifdef MKXPZ_RETRO
#  include "sandbox-serial-util.h"
#endif // MKXPZ_RETRO

#define GUARD_V(value, expression) do { expression; if (exception.is_error()) return value; } while (0)
#define GUARD(expression) GUARD_V(, expression)

Scene::Scene()
{}

Scene::~Scene()
{
	/* Ensure elements don't unlink from a destructed Scene */
	IntruListLink<SceneElement> *iter;

	for (iter = elements.begin(); iter != elements.end(); iter = iter->next)
	{
		iter->data->scene = 0;
#ifdef MKXPZ_RETRO
		iter->data->deserSceneElementWasUnlinked = true;
#endif // MKXPZ_RETRO
	}
}

void Scene::insert(SceneElement &element)
{
	IntruListLink<SceneElement> *iter;

	for (iter = elements.begin(); iter != elements.end(); iter = iter->next)
	{
		SceneElement *e = iter->data;

		if (element < *e)
		{
			elements.insertBefore(element.link, *iter);
			return;
		}
	}

	elements.append(element.link);
}

void Scene::insertAfter(SceneElement &element, SceneElement &after)
{
	IntruListLink<SceneElement> *iter;

	for (iter = &after.link; iter != elements.end(); iter = iter->next)
	{
		SceneElement *e = iter->data;

		if (element < *e)
		{
			elements.insertBefore(element.link, *iter);
			return;
		}
	}

	elements.append(element.link);
}

void Scene::reinsert(SceneElement &element)
{
	elements.remove(element.link);
#ifdef MKXPZ_RETRO
	element.deserSceneElementWasUnlinked = true;
#endif // MKXPZ_RETRO
	insert(element);
}

void Scene::notifyGeometryChange()
{
	IntruListLink<SceneElement> *iter;

	for (iter = elements.begin(); iter != elements.end(); iter = iter->next)
	{
		iter->data->onGeometryChange(geometry);
	}
}

void Scene::composite(Exception &exception)
{
	IntruListLink<SceneElement> *iter;

	for (iter = elements.begin(); iter != elements.end(); iter = iter->next)
	{
		SceneElement *e = iter->data;

		if (e->visible)
			GUARD(e->draw(exception));
	}
}


SceneElement::SceneElement(Scene &scene, int z, int spriteY)
    : scene(&scene),
      link(this),
      creationStamp(shState->genTimeStamp()),
      z(z),
      visible(true),
      spriteY(spriteY)
{
	scene.insert(*this);
}

SceneElement::~SceneElement()
{
	unlink();
}

void SceneElement::setScene(Scene &scene)
{
	unlink();

	this->scene = &scene;

	scene.insert(*this);

	onGeometryChange(scene.getGeometry());
}

int SceneElement::getZ(Exception &exception) const
{
	GUARD_V(0, aboutToAccess(exception));

	return z;
}

void SceneElement::setZ(Exception &exception, int value)
{
	GUARD(aboutToAccess(exception));

	if (z == value)
		return;

	z = value;
	scene->reinsert(*this);
}

bool SceneElement::getVisible(Exception &exception) const
{
	GUARD_V(false, aboutToAccess(exception));

	return visible;
}

void SceneElement::setVisible(Exception &exception, bool value)
{
	GUARD(aboutToAccess(exception));

	visible = value;
}

bool SceneElement::operator<(const SceneElement &o) const
{
	/* Element draw order is decided by their Z value.
	 * If two Z values are equal, the later created object
	 * has priority */

	if (z <= o.z)
	{
		if (z == o.z)
		{
			if (rgssVer >= 2)
			{
				/* RGSS2: If two sprites' Z values collide,
				 * their Y coordinates decide draw order. Only
				 * on equal Y does the creation time take effect */
				if (spriteY != o.spriteY)
					return (spriteY < o.spriteY);
			}

			return (creationStamp < o.creationStamp);
		}

		return true;
	}

	return false;
}

void SceneElement::setSpriteY(int value)
{
	spriteY = value;
	scene->reinsert(*this);
}

void SceneElement::unlink()
{
	if (scene)
		scene->elements.remove(link);

#ifdef MKXPZ_RETRO
	deserSceneElementWasUnlinked = true;
#endif // MKXPZ_RETRO
}

#ifdef MKXPZ_RETRO
#ifndef MKXPZ_SANDBOX_SERIAL_SCENE_H
#define MKXPZ_SANDBOX_SERIAL_SCENE_H
#include "sandbox-serial-scene.h"
#endif // MKXPZ_SANDBOX_SERIAL_SCENE_H
#endif // MKXPZ_REROO
