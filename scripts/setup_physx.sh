#!/bin/sh
set -e
cd "$(dirname "$0")/.."

PHYSX_COMMIT=6b3a94534016270f8f6ebc665cd97ba7ce858d84

if [ -f 3rd-party/physx/physx/include/PxPhysicsAPI.h ]; then
	echo "PhysX SDK already present."
	exit 0
fi

echo "=== Fetching PhysX 4.1 SDK ==="
if [ ! -d 3rd-party/physx-rs ]; then
	git clone https://github.com/EmbarkStudios/physx-rs 3rd-party/physx-rs
fi
git -C 3rd-party/physx-rs fetch origin "$PHYSX_COMMIT"
git -C 3rd-party/physx-rs -c advice.detachedHead=false checkout "$PHYSX_COMMIT"
if [ ! -f 3rd-party/physx-rs/physx-sys/PhysX/physx/include/PxPhysicsAPI.h ]; then
	git -C 3rd-party/physx-rs submodule update --init physx-sys/PhysX
fi
cp -r 3rd-party/physx-rs/physx-sys/PhysX 3rd-party/physx
echo "Done, PhysX SDK is at 3rd-party/physx"
