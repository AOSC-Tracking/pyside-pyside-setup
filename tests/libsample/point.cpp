/*
 * This file is part of the Shiboken Python Binding Generator project.
 *
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Contact: PySide team <contact@pyside.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation. Please
 * review the following information to ensure the GNU Lesser General
 * Public License version 2.1 requirements will be met:
 * http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 *
 * As a special exception to the GNU Lesser General Public License
 * version 2.1, the object code form of a "work that uses the Library"
 * may incorporate material from a header file that is part of the
 * Library.  You may distribute such object code under terms of your
 * choice, provided that the incorporated material (i) does not exceed
 * more than 5% of the total size of the Library; and (ii) is limited to
 * numerical parameters, data structure layouts, accessors, macros,
 * inline functions and templates.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <iostream>
#include "point.h"

using namespace std;

Point::Point(int x, int y) : m_x(x), m_y(y)
{
}

Point::Point(double x, double y) : m_x(x), m_y(y)
{
}

Point*
Point::copy() const
{
    Point* pt = new Point();
    pt->m_x = m_x;
    pt->m_y = m_y;
    return pt;
}

void
Point::show()
{
    cout << "(x: " << m_x << ", y: " << m_y << ")";
}

bool
Point::operator==(const Point& other)
{
    return m_x == other.m_x && m_y == other.m_y;
}

Point
Point::operator+(const Point& other)
{
    return Point(m_x + other.m_x, m_y + other.m_y);
}

Point
Point::operator-(const Point& other)
{
    return Point(m_x - other.m_x, m_y - other.m_y);
}

Point&
Point::operator+=(Point &other)
{
    m_x += other.m_x;
    m_y += other.m_y;
    return *this;
}

bool
Point::operator!=(const Point &other)
{
    return (m_x != other.m_x) || (m_y != other.m_y);
}

Point&
Point::operator-=(Point &other)
{
    m_x -= other.m_x;
    m_y -= other.m_y;
    return *this;
}

Point
operator*(const Point& pt, double mult)
{
    return Point(pt.m_x * mult, pt.m_y * mult);
}

Point
operator*(const Point& pt, int mult)
{
    return Point(((int) pt.m_x) * mult, ((int) pt.m_y) * mult);
}

Point
operator*(double mult, const Point& pt)
{
    return Point(pt.m_x * mult, pt.m_y * mult);
}

Point
operator*(int mult, const Point& pt)
{
    return Point(((int) pt.m_x) * mult, ((int) pt.m_y) * mult);
}

Point
operator-(const Point& pt)
{
    return Point(-pt.m_x, -pt.m_y);
}

bool
operator!(const Point& pt)
{
    return (pt.m_x == 0.0 && pt.m_y == 0.0);
}

Complex
transmutePointIntoComplex(const Point& point)
{
    Complex cpx(point.x(), point.y());
    return cpx;
}

Point
transmuteComplexIntoPoint(const Complex& cpx)
{
    Point pt(cpx.real(), cpx.imag());
    return pt;
}

