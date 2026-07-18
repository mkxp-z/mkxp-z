/*
** sceneelement-binding.h
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

#ifndef SCENEELEMENTBINDING_H
#define SCENEELEMENTBINDING_H

#include "scene.h"
#include "binding-util.h"
#include "graphics.h"

template<class C>
RB_METHOD_GUARD(sceneElementGetZ)
{
	RB_UNUSED_PARAM;

	SceneElement *se = getPrivateData<C>(self);

	int value = 0;
	value = se->getZ();

	return rb_fix_new(value);
}
RB_METHOD_GUARD_END

template<class C>
RB_METHOD_GUARD(sceneElementSetZ)
{
	SceneElement *se = getPrivateData<C>(self);

	int z;
	rb_get_args(argc, argv, "i", &z RB_ARG_END);

	GFX_GUARD;
	se->setZ(z); 

	return rb_fix_new(z);
}
RB_METHOD_GUARD_END

template<class C>
RB_METHOD_GUARD(sceneElementGetVisible)
{
	RB_UNUSED_PARAM;

	SceneElement *se = getPrivateData<C>(self);

	bool value = false;
	value = se->getVisible();

	return rb_bool_new(value);
}
RB_METHOD_GUARD_END

template<class C>
RB_METHOD_GUARD(sceneElementSetVisible)
{
	SceneElement *se = getPrivateData<C>(self);

	bool visible;
	rb_get_args(argc, argv, "b", &visible RB_ARG_END);

	GFX_GUARD;
	se->setVisible(visible); 

	return rb_bool_new(visible);
}
RB_METHOD_GUARD_END

template<class C>
void
sceneElementBindingInit(VALUE klass)
{
	_rb_define_method(klass, "z",        sceneElementGetZ<C>);
	_rb_define_method(klass, "z=",       sceneElementSetZ<C>);
	_rb_define_method(klass, "visible",  sceneElementGetVisible<C>);
	_rb_define_method(klass, "visible=", sceneElementSetVisible<C>);
}

#endif // SCENEELEMENTBINDING_H
