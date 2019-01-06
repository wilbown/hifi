//
//  GymAC.js
//
//  Script Type: AC
//  Created by Wil Bown on 1/3/19
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

// file:///home/wilbown/GitHub/hifi/script-archive/acScripts/GymAC.js

function getRndInt (min, max) { return Math.floor(Math.random() * (max - min + 1)) + min; }
function getRndFloat(min, max) { return Math.random() * (max - min) + min; }

// Agent.isAvatar = true; // getAvatarsInRange detects this script's Avatar if this is true
var POS_ZERO = { x: 0, y: 0, z: 0 };
var DIST_MAX = 40;
var DIST_NEAR = 5;

// Setup EntityViewer
EntityViewer.setPosition(POS_ZERO);
EntityViewer.setOrientation(Quat.fromPitchYawRollDegrees(0, -90, 0));
// EntityViewer.setCenterRadius(10);

// var timePassed = 0.0;
// var updateSpeed = 5.0; // seconds
// function update(deltaTime) {
//     timePassed += deltaTime;
//     if (timePassed > updateSpeed) {
//         timePassed = 0.0;
//         EntityViewer.queryOctree();
//         // print("GymAC::update NEW");
//         // print("GymAC::update path: " + Script.resolvePath("/")); // "file://"
//         // print("GymAC::update path: " + Script.resolvePath("")); // file:///home/wilbown/GitHub/hifi/script-archive/acScripts/GymAC.js


//         // var position = Avatar.position;
//         // print("GymAC.update Avatar.position: " + JSON.stringify(position)); //{"x":0,"y":0,"z":0}

//         // var props = Entities.getEntityProperties("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}"); // gymSphere id
//         // print("GymAC.update Entities.position: " + JSON.stringify(props));

//         // var entityIDs = Entities.findEntities(position, 1000);
//         // var entityIDs = Entities.findEntities(0, 1000);
//         // var entityIDs = Entities.findEntities([0, 0, 0], 10);
//         // var entityIDs = Entities.findEntitiesByType("Shape", position, 1000);

//         // var entityIDs = AvatarList.getAvatarsInRange([0, 0, 0], 10); //works
//         // // var entityIDs = AvatarList.getAvatarIdentifiers(); //works, gets all on server
//         // // print("GymAC::update Entities.num entities: " + entityIDs.length);
//         // for (var key in entityIDs) {
//         //     var type = entityIDs[key];
//         //     // var type = Entities.getEntityProperties(entityIDs[key]).position;
//         //     // var type = AvatarList.getAvatar(entityIDs[key]);
//         //     print("GymAC::update Entities.num entities.type: " + JSON.stringify(type));
//         // }

//         // var props = Entities.getEntityProperties("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}");
//         // if (Object.keys(props).length !== 0) {
//         //     // print("GymAC.update Entities.position: " + JSON.stringify(props));
//         //     // Entities.editEntity("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}", { color: { red: getRndInt(0,255), green: getRndInt(0,255), blue: getRndInt(0,255)} });
//         //     // Entities.editEntity("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}", { userData: "testing" });
//         //     // Entities.editEntity("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}", { userData: JSON.stringify({ observation: [getRndFloat(-1.0,1.0), getRndFloat(-1.0,1.0), getRndFloat(-1.0,1.0), -0.35], reward: 0.03, done: false, info: {error: null}}) });
//         //     Entities.editEntity("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}", { userData: JSON.stringify({ observation: [getRndFloat(-1.0,1.0), getRndFloat(-1.0,1.0), getRndFloat(-1.0,1.0), -0.35], reward: 0.03}) });
//         // }

//         var props = Entities.getEntityProperties("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}");
//         if (Object.keys(props).length !== 0) {
//             var rwd = 0.0;
//             var obs = [1.0, 1.0, 1.0, 1.0];
//             var obsi = 0;
//             var entityIDs = AvatarList.getAvatarsInRange(props.position, 10); //works
//             for (i = 0; i < entityIDs.length; i++) {
//                 var avtr = AvatarList.getAvatar(entityIDs[i]);
//                 if (Object.keys(avtr).length !== 0) {
//                     var dist = Vec3.distance(props.position, avtr.position);
//                     if (dist > 10) continue;
//                     if (dist < 0.5) rwd += 1.0;
//                     if (dist < 0.0) dist = 0.0;
//                     obs[obsi] = dist / 5.0 - 1.0; obsi++;
//                 }
//             }
//             Entities.editEntity("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}", { userData: JSON.stringify({ observation: obs, reward: rwd}) });
//         }

//     }
// }


function update(deltaTime) {
    EntityViewer.queryOctree();

    var props = Entities.getEntityProperties("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}");
    if (Object.keys(props).length !== 0) {
        var rwd = 0.0;
        var obs = [];
        var obsi = 0;
        var entityIDs = AvatarList.getAvatarsInRange(props.position, DIST_MAX);
        for (i = 0; i < entityIDs.length; i++) {
            var avtr = AvatarList.getAvatar(entityIDs[i]);
            if (Object.keys(avtr).length !== 0) {
                var dist = Vec3.distance(props.position, avtr.position);
                if (dist > DIST_MAX) continue;
                if (dist < 0.5) rwd -= 1.0;
                else if (dist < 1.5) rwd += 0.2;
                if (dist < 0.0) dist = 0.0;
                var near = (dist > DIST_NEAR) ? 255 : dist/DIST_NEAR*255;
                var far = dist/DIST_MAX*255;
                var rads = Vec3.getAngle(props.position, avtr.position);
                // print("GymAC::update rads: " + rads);
                var angle = rads/Math.PI*255;
                if (angle > 255) angle = 255;
                obs[obsi++] = 255 - parseInt(near);
                obs[obsi++] = 255 - parseInt(far);
                obs[obsi++] = parseInt(angle);
                if (obsi > 4*4*3) break; //max observation size
            }
        }
        Entities.editEntity("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}", { userData:JSON.stringify({ reward:rwd, observation:obs }) });
    }
}

function unload() {
    Script.update.disconnect(update);
}

Script.update.connect(update);
Script.scriptEnding.connect(unload);

// getAvatar = {"objectName":"",
// "position":{"x":-0.05963579937815666,"y":1.0846925973892212,"z":-0.2838750183582306},
// "scale":0.9283730387687683,
// "handPosition":{"x":-0.05963579937815666,"y":1.0846925973892212,"z":-0.2838750183582306},
// "bodyPitch":0.0018089902587234974,"bodyYaw":-60.243534088134766,"bodyRoll":0.0018089902587234974,
// "orientation":{"x":-0.000021576881408691406,"y":0.5018393397331238,"z":-0.000021576881408691406,"w":-0.8649608492851257},
// "headOrientation":{"x":-0.000021576881408691406,"y":0.5018393397331238,"z":-0.000021576881408691406,"w":-0.8649608492851257},
// "headPitch":0,"headYaw":0,"headRoll":0,
// "velocity":{"x":0,"y":0,"z":0},"angularVelocity":{"x":0,"y":0,"z":0},
// "sessionUUID":"{8f0ca9d4-feab-4f9c-b9c1-2cf5416dcc94}",
// "displayName":"Wil", "sessionDisplayName":"",
// "isReplicated":false,
// "lookAtSnappingEnabled":true,
// "skeletonModelURL":"https://s3-us-west-1.amazonaws.com/hifi-wilbown/avatar-BeingOfLight-mod/being_of_light.fst",
// "attachmentData":[],"jointNames":[],
// "audioLoudness":0,"audioAverageLoudness":0,
// "sensorToWorldMatrix":{"r0c0":0.5108550190925598,"r1c0":0.000016129144569276832,"r2c0":0.8935766220092773,"r3c0":0,"r0c1":-0.00006071057578083128,"r1c1":1.029296875,"r2c1":0.000016129144569276832,"r3c1":0,"r0c2":-0.8935766220092773,"r1c2":-0.00006071057578083128,"r2c2":0.5108550190925598,"r3c2":0,"r0c3":-0.061158500611782074,"r1c3":-0.17988932132720947,"r2c3":-0.28300338983535767,"r3c3":1},
// "controllerLeftHandMatrix":{"r0c0":1,"r1c0":0.00004315469413995743,"r2c0":-0.0000431528314948082,"r3c0":0,"r0c1":-0.0000431528314948082,"r1c1":1,"r2c1":0.00004315469413995743,"r3c1":0,"r0c2":0.00004315469413995743,"r1c2":-0.0000431528314948082,"r2c2":1,"r3c2":0,"r0c3":0,"r1c3":0,"r2c3":0,"r3c3":1},
// "controllerRightHandMatrix":{"r0c0":1,"r1c0":0.00004315469413995743,"r2c0":-0.0000431528314948082,"r3c0":0,"r0c1":-0.0000431528314948082,"r1c1":1,"r2c1":0.00004315469413995743,"r3c1":0,"r0c2":0.00004315469413995743,"r1c2":-0.0000431528314948082,"r2c2":1,"r3c2":0,"r0c3":0,"r1c3":0,"r2c3":0,"r3c3":1}}

// getEntityProperties = {"id":"{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}",
// "type":"Sphere",
// "created":"2019-01-03T08:52:31Z",
// "age":67172.65625,"ageAsText":"18 hours 39 minutes 32 seconds","lastEdited":1546572696951929,"lastEditedBy":"{ec1d12a0-f832-4edd-a25a-5cd573d4da8e}",
// "position":{"x":1.417858600616455,"y":1.280480146408081,"z":-1.5713622570037842},
// "dimensions":{"x":0.5,"y":0.5,"z":0.5},
// "naturalDimensions":{"x":1,"y":1,"z":1},"naturalPosition":{"x":0,"y":0,"z":0},
// "rotation":{"x":-0.0000152587890625,"y":-0.0000152587890625,"z":-0.0000152587890625,"w":1},
// "velocity":{"x":0,"y":0,"z":0},"gravity":{"x":0,"y":0,"z":0},"acceleration":{"x":0,"y":0,"z":0},"damping":0.39346998929977417,"restitution":0.5,"friction":0.5,"density":1000,
// "lifetime":-1,
// "script":"","scriptTimestamp":0,
// "serverScripts":"file:///home/wilbown/GitHub/hifi/scripts/tutorials/entity_scripts/gymSphere.js",
// "registrationPoint":{"x":0.5,"y":0.5,"z":0.5},
// "angularVelocity":{"x":0,"y":0,"z":0},"angularDamping":0.39346998929977417,
// "visible":true,"canCastShadow":true,
// "collisionless":true,"ignoreForCollisions":true,"collisionMask":31,"collidesWith":"static,dynamic,kinematic,myAvatar,otherAvatar,","dynamic":false,"collisionsWillMove":false,
// "href":"","description":"","actionData":"",
// "locked":false,
// "userData":"",
// "alpha":1,
// "itemName":"",
// "itemDescription":"",
// "itemCategories":"",
// "itemArtist":"","itemLicense":"","limitedRun":4294967295,"marketplaceID":"","editionNumber":0,"entityInstanceNumber":0,"certificateID":"","staticCertificateVersion":0,
// "name":"Gym Sphere",
// "collisionSoundURL":"",
// "shape":"Sphere",
// "color":{"red":47,"green":241,"blue":189},
// "boundingBox":{"brn":{"x":1.1678433418273926,"y":1.0304648876190186,"z":-1.8213775157928467},"tfl":{"x":1.6678738594055176,"y":1.5304954051971436,"z":-1.3213469982147217},"center":{"x":1.417858600616455,"y":1.280480146408081,"z":-1.5713622570037842},"dimensions":{"x":0.500030517578125,"y":0.500030517578125,"z":0.500030517578125}},
// "originalTextures":"{\n}\n",
// "parentID":"{00000000-0000-0000-0000-000000000000}","parentJointIndex":65535,
// "queryAACube":{"x":0.9848458766937256,"y":0.8474674224853516,"z":-2.0043749809265137,"scale":0.8660253882408142},
// "localPosition":{"x":1.417858600616455,"y":1.280480146408081,"z":-1.5713622570037842},
// "localRotation":{"x":-0.0000152587890625,"y":-0.0000152587890625,"z":-0.0000152587890625,"w":1},
// "localVelocity":{"x":0,"y":0,"z":0},"localAngularVelocity":{"x":0,"y":0,"z":0},
// "localDimensions":{"x":0.5,"y":0.5,"z":0.5},
// "entityHostType":"domain",
// "owningAvatarID":"{00000000-0000-0000-0000-000000000000}",
// "cloneable":false,"cloneLifetime":300,"cloneLimit":0,"cloneDynamic":false,"cloneAvatarEntity":false,"cloneOriginID":"{00000000-0000-0000-0000-000000000000}",
// "grab":{"grabbable":false,"grabKinematic":true,"grabFollowsController":true,"triggerable":true,"equippable":false,"equippableLeftPosition":{"x":0,"y":0,"z":0},"equippableLeftRotation":{"x":-0.0000152587890625,"y":-0.0000152587890625,"z":-0.0000152587890625,"w":1},"equippableRightPosition":{"x":0,"y":0,"z":0},"equippableRightRotation":{"x":-0.0000152587890625,"y":-0.0000152587890625,"z":-0.0000152587890625,"w":1},"equippableIndicatorURL":"","equippableIndicatorScale":{"x":1,"y":1,"z":1},"equippableIndicatorOffset":{"x":0,"y":0,"z":0}},
// "renderInfo":{},
// "clientOnly":false,"avatarEntity":false,"localEntity":false}
