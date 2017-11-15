#include "camera.h"
#include <glm/gtc/matrix_transform.hpp>

Camera::Camera(glm::mat4* view)
	: _azimuth(glm::radians(-90.f)),
	_altitude(glm::radians(0.f)),
	_radius(5.f),
	_center(0, 0, 0),
	_dirty(true),
	_view(*view) {
	recalculate();
}

void Camera::rotate(float dAzimuth, float dAltitude) {
	_dirty = true;
	_azimuth = glm::mod(_azimuth + dAzimuth, glm::radians(360.f));
	_altitude = glm::clamp(_altitude + dAltitude, glm::radians(-89.f), glm::radians(89.f));
}

void Camera::pan(float dX, float dY) {
	recalculate();
	_dirty = true;
	glm::vec3 vX = glm::normalize(glm::cross(-_eyeDir, glm::vec3(0, 1, 0)));
	glm::vec3 vY = glm::normalize(glm::cross(_eyeDir, vX));
	_center += vX * dX * _radius + vY * dY * _radius;
}

void Camera::zoom(float factor) {
	_dirty = true;
	_radius = _radius * glm::exp(-factor);
}

const glm::mat4& Camera::view() {
	if (_dirty) {
		recalculate();
	}
	return _view;
}

void Camera::recalculate() {
	if (_dirty) {
		glm::vec4 eye4 = glm::vec4(1, 0, 0, 1);
		glm::mat4 r1 = glm::rotate(glm::mat4(1.0), _altitude, glm::vec3(0, 0, 1));
		glm::mat4 r2 = glm::rotate(glm::mat4(1.0), _azimuth, glm::vec3(0, 1, 0));
		eye4 = r2 * r1 * eye4;
		_eyeDir = glm::vec3(eye4);

		_view = glm::lookAt(_center + _eyeDir * _radius, _center, glm::vec3(0, 1, 0));
		_dirty = false;
	}
}