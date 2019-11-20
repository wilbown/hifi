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

function getRndInt(min, max) { return Math.floor(Math.random() * (max - min + 1)) + min; }
function getRndFloat(min, max) { return Math.random() * (max - min) + min; }

var agent = 5558;
var obs_size = 4*4*3; //max observation size

var actorID = "{177dfde6-8dbd-4f3e-8efd-88684b1c54b8}";
// var platformID = "{71fecec1-318c-472c-b8ab-14dbf8a59efb}";
// var platformArtID = "{bbd37eca-9650-4b8d-a063-fac2d3150ebb}";
// var platformPeople = "{71fecec1-318c-472c-b8ab-14dbf8a59efb}";

// Agent.isAvatar = true; // getAvatarsInRange detects this script's Avatar if this is true

// var ROT_PLATFORM = 1.0;
var ROT_PLATFORM_ART = Quat.fromPitchYawRollDegrees(0, 40, 0);

var DIST_MAX = 3
var DIST_HEIGHT_MAX = 3
var DIST_HEIGHT_MIN = 0.05

var MOVE_X = { x: 0.01, y: 0, z: 0 };
var MOVE_Y = { x: 0, y: 0.01, z: 0 };
var MOVE_Z = { x: 0, y: 0, z: 0.01 };
var MOVE_FAST_X = { x: 0.1, y: 0, z: 0 };
var MOVE_FAST_Z = { x: 0, y: 0, z: 0.1 };

var ROT_Xl = Quat.fromPitchYawRollDegrees(15, 0, 0);
var ROT_XR = Quat.fromPitchYawRollDegrees(-15, 0, 0);
var ROT_Yl = Quat.fromPitchYawRollDegrees(0, 15, 0);
var ROT_YR = Quat.fromPitchYawRollDegrees(0, -15, 0);
var ROT_Zl = Quat.fromPitchYawRollDegrees(0, 0, 15);
var ROT_ZR = Quat.fromPitchYawRollDegrees(0, 0, -15);

var AVATAR_DIST_MAX = 30;
var AVATAR_DIST_NEAR = 5;

var MAX_OBJ_PER_ART = 30;

// var SCORE_WAIT = 10000;

// // var COLOR_TEAL = { red: 0, green: 255, blue: 255 };
// // var COLOR_YELLOW = { red: 255, green: 255, blue: 0 };
// var COLOR_MAD = { red: 255, green: 0, blue: 0 };
// var COLOR_HAPPY = { red: 0, green: 255, blue: 0 };
// var COLOR_COOL = { red: 0, green: 0, blue: 255 };
var COLOR_PINK = { red: 255, green: 192, blue: 203 };

// var SHAPES = ["Circle","Cone","Cube","Cylinder","Dodecahedron","Hexagon","Icosahedron","Octagon","Octahedron","Quad","Sphere","Tetrahedron","Triangle"];
var SHAPES = ["Cube","Cylinder","Cone","Sphere","Dodecahedron","Octahedron","Tetrahedron"];

var POS_ZERO = { x: 0, y: 1.0, z: 0 };
var COLOR_ZERO = { red: 127, green: 127, blue: 127 };
var SHAPE_ZERO = "Sphere";


// Setup EntityViewer
EntityViewer.setPosition(POS_ZERO);
// EntityViewer.setOrientation(Quat.fromPitchYawRollDegrees(0, -90, 0));
EntityViewer.setCenterRadius(1000);




// var timePassed = 0.0;
// var updateSpeed = 5.0; // seconds
// var oneoff = true;
// var tick = 0;
// function update(deltaTime) {
//     timePassed += deltaTime;
//     if (timePassed > updateSpeed) {
//         timePassed = 0.0;
//         if (!Entities.serversExist() || !Entities.canRez()) return;
//         EntityViewer.queryOctree();
//         // print("GymAC.update NEW");
//         // print("GymAC.update path: " + Script.resolvePath("/")); // "file://"
//         // print("GymAC.update path: " + Script.resolvePath("")); // file:///home/wilbown/GitHub/hifi/script-archive/acScripts/GymAC.js

//         // print("GymAC.update Avatar.position: " + JSON.stringify(Avatar.position)); //{"x":0,"y":0,"z":0}
//         // print("GymAC.update Avatar.sessionUUID: " + JSON.stringify(Avatar.sessionUUID)); //"{ab017ab6-f53c-4af4-b3ff-6118ae98181f}"

//         var props = Entities.getEntityProperties(this.actorID);
//         // print("GymAC.update Entities.props: " + JSON.stringify(props));



//         var parentID = Entities.addEntity({
//             // visible: false,
//             name: "DerpArt"+tick,
//             type: "Shape",
//             shape: "Cylinder",
//             position: { x: 0.0, y: 0.0, z: 0.0 },
//             dimensions: { x: 5.0, y: 0.1, z: 5.0 },
//             color: { red: 0, green: 255, blue: 0 },
//             alpha: 1.0,
//             lifetime: 45
//         });
//         // var parent = Entities.getEntityProperties(parentID);
//         // print("Entity created: " + JSON.stringify(parent));

//         var entityID = Entities.addEntity({
//             parentID: parentID,
//             type: "Shape",
//             shape: "Cube", // "Circle" "Cone" "Cube" "Cylinder" "Dodecahedron" "Hexagon" "Icosahedron" "Octagon" "Octahedron" "Quad" "Sphere" "Tetrahedron" "Triangle"
//             localPosition: Vec3.sum(props.position, Vec3.multiplyQbyV(props.rotation, { x: 0.1*tick, y: 0, z: -0.1*tick })),
//             localRotation: props.rotation,
//             dimensions: { x: 0.2, y: 0.2, z: 0.2 },
//             color: { red: 0, green: 0, blue: 255 },
//             alpha: 1.0,
//             lifetime: 45
//         });
//         // var entity = Entities.getEntityProperties(entityID);
//         // print("Entity created: " + JSON.stringify(entity));

//         var q = Quat.fromPitchYawRollDegrees(0, 40, 0);
//         for (t = 0; t < tick; t++) {
//             var entityIDs = Entities.findEntitiesByName("DerpArt"+t, POS_ZERO, 100, false);
//             // print("Entity found: " + entityIDs.length);
//             for (i = 0; i < entityIDs.length; i++) {
//                 var entity = Entities.getEntityProperties(entityIDs[i]);
//                 newpos = Vec3.sum(entity.position, { x: 0, y: 1.0, z: 0 });
//                 if (t == tick - 1) newpos = Vec3.sum(newpos, { x: 0, y: 0, z: 8.0 });
//                 newpos = Vec3.multiplyQbyV(q, newpos);
//                 Entities.editEntity(entityIDs[i], { position: newpos });
//             }
//         }


//         // var props = Entities.getEntityProperties("{ddc4fba8-6e36-42d7-af5a-3cf9b39fdcff}");
//         // print("GymAC.update Entities.position: " + JSON.stringify(props));

//         // var entityIDs = Entities.findEntities(position, 1000);
//         // var entityIDs = Entities.findEntities(0, 1000);
//         // var entityIDs = Entities.findEntities([0, 0, 0], 1000);
//         // var entityIDs = Entities.findEntitiesByType("Shape", position, 1000);
//         // var entityIDs = Entities.findEntitiesByName("Light-Target", position, 10, false);

//         // var entityIDs = AvatarList.getAvatarsInRange([0, 0, 0], 10); //works
//         // var entityIDs = AvatarList.getAvatarIdentifiers(); //works, gets all on server
//         // print("GymAC.update Entities.num entities: " + entityIDs.length);
//         // var count = 0;
//         // for (i = 0; i < entityIDs.length; i++) {
//         //     var id = entityIDs[i];
//         //     var entity = Entities.getEntityProperties(entityIDs[i]);
//         //     // var avtr = AvatarList.getAvatar(entityIDs[i]);
//         //     if (Object.keys(entity).length === 0) continue;
//         //     if (!entity.locked && entityIDs[i]!=this.actorID) {
//         //         count++;
//         //         // Entities.deleteEntity(entityIDs[i]);
//         //         // print("GymAC.update deleted:" + id);
//         //         break;
//         //     }
//         //     // print("GymAC.update Entities: " + JSON.stringify(entity.location));
//         // }
//         // print("GymAC.update Entities: " + count);

//         // var props = Entities.getEntityProperties(this.actorID);
//         // if (Object.keys(props).length !== 0) {
//         //     // print("GymAC.update Entities.position: " + JSON.stringify(props));
//         //     // Entities.editEntity(this.actorID, { color: { red: getRndInt(0,255), green: getRndInt(0,255), blue: getRndInt(0,255)} });
//         //     // Entities.editEntity(this.actorID, { userData: "testing" });
//         //     // Entities.editEntity(this.actorID, { userData: JSON.stringify({ observation: [getRndFloat(-1.0,1.0), getRndFloat(-1.0,1.0), getRndFloat(-1.0,1.0), -0.35], reward: 0.03, done: false, info: {error: null}}) });
//         //     Entities.editEntity(this.actorID, { userData: JSON.stringify({ observation: [getRndFloat(-1.0,1.0), getRndFloat(-1.0,1.0), getRndFloat(-1.0,1.0), -0.35], reward: 0.03}) });
//         // }

//         // var props = Entities.getEntityProperties(this.actorID);
//         // if (Object.keys(props).length !== 0) {
//         //     var rwd = 0.0;
//         //     var obs = [1.0, 1.0, 1.0, 1.0];
//         //     var obsi = 0;
//         //     var entityIDs = AvatarList.getAvatarsInRange(props.position, 10); //works
//         //     for (i = 0; i < entityIDs.length; i++) {
//         //         var avtr = AvatarList.getAvatar(entityIDs[i]);
//         //         if (Object.keys(avtr).length !== 0) {
//         //             var dist = Vec3.distance(props.position, avtr.position);
//         //             if (dist > 10) continue;
//         //             if (dist < 0.5) rwd += 1.0;
//         //             if (dist < 0.0) dist = 0.0;
//         //             obs[obsi] = dist / 5.0 - 1.0; obsi++;
//         //         }
//         //     }
//         //     Entities.editEntity(this.actorID, { userData: JSON.stringify({ observation: obs, reward: rwd}) });
//         // }

//         if (oneoff) {
//             oneoff = false;

//             // var entityID = Entities.addEntity({
//             //     // visible: false,
//             //     name: "DerpArt1",
//             //     type: "Sphere",
//             //     position: POS_ZERO,
//             //     dimensions: { x: 1.0, y: 1.0, z: 1.0 },
//             //     color: { red: 0, green: 255, blue: 0 },
//             //     alpha: 1.0,
//             //     lifetime: 45
//             // });
//             // var entity = Entities.getEntityProperties(entityID);
//             // print("Entity created: " + JSON.stringify(entity));


//         }

//         tick++;
//     }
// }





var environment = { // observation = object, info = dictionary
    observation: [],
    reward: 0.0,
    done: false,
    info: {scoringwait: 0, scoring: false, saveart: false, paintparent:false, artindex: 0, numobj: 0, error: null},
}
var oneshot = true;
var totalTime = 0.0;

function update(deltaTime) {
    if (!Entities.serversExist() || !Entities.canRez()) return;
    EntityViewer.queryOctree();

    if (this.oneshot) {
        this.oneshot = false;

        // var entityIDs = Entities.findEntitiesByName("DerpQ", POS_ZERO, 1000, true);
        // if (entityIDs.length > 0) {
        //     print("GymAC.update Entities found: " + entityIDs);
        //     var props = Entities.getEntityProperties(entityIDs[0]);
        //     print("GymAC.update Entities.props: " + JSON.stringify(props));
        // }
        // else this.oneshot = true;
    }

    // animations
    // var platform = Entities.getEntityProperties(platformID);
    // Entities.editEntity(platformID, { rotation: Quat.multiply(Quat.fromPitchYawRollDegrees(0, ROT_PLATFORM * deltaTime, 0), platform.rotation) });


    // get environment and rewards
    var actor = Entities.getEntityProperties(this.actorID);
    if (Object.keys(actor).length === 0) return; // actor does not exist yet

    var rwd = 0.0;
    var obs = [];
    var obsi = 0;

    var scoring = this.environment.info.scoring;
    // var scoring = (Date.now() < this.environment.info.scoring);
    // if (!scoring && this.environment.info.scoring > 0) {
    //     this.environment.info.scoring = 0;
    //     this.environment.done = true;
    // }
    // print("GymAC.update: scoring " + this.environment.info.scoring);

    // Add actor position
    var edist = Vec3.distance(actor.position, POS_ZERO);
    var erads = Vec3.getAngle(actor.position, POS_ZERO);
    var epose = getPose(edist, erads);
    epose.forEach(function(e){ obs[obsi++] = e; });
    // print("GymAC.update: epose " + epose);

    // Out of bounds
    if (edist > DIST_MAX || actor.position.y > DIST_HEIGHT_MAX || actor.position.y < DIST_HEIGHT_MIN) {
        // print("GymAC.update: OUT OF BOUNDS " + edist);
        rwd += edist * ((scoring)?-3.0:-1.0);
        // this.environment.done = true;
        // Entities.editEntity(this.actorID, { position: Vec3.subtract(actor.position, Vec3.normalize(actor.position)) });
    }

    // See local avatars
    var avatarIDs = AvatarList.getAvatarsInRange(actor.position, AVATAR_DIST_MAX);
    // print("GymAC.update AvatarList.find length: " + avatarIDs.length);
    for (i = 0; i < avatarIDs.length; i++) {
        var avtr = AvatarList.getAvatar(avatarIDs[i]);
        if (Object.keys(avtr).length === 0) continue; // avatar doesn't exist?

        var dist = Vec3.distance(actor.position, avtr.position);
        if (dist > AVATAR_DIST_MAX) continue;
        var rads = Vec3.getAngle(actor.position, avtr.position);

        if (dist < 0.2) {
            rwd += (scoring)?0.0:-5.0;
            // this.environment.done = true;
            // Entities.editEntity(this.actorID, { position: Vec3.subtract(actor.position, Vec3.normalize(actor.position)) });
        }
        else if (dist < 0.6) rwd += (scoring)?20.0:-1.0;
        else if (dist < 1.5) rwd += (scoring)?0.0:0.2;
        else if (dist < 5) rwd += (scoring)?0.0:0.03;
        else if (dist < 10) rwd += (scoring)?0.0:0.01;

        var pose = getPose(dist, rads);
        pose.forEach(function(e){ obs[obsi++] = e; });
        if (obsi > obs_size) break;
    }

    if (obsi < obs_size - 3) {
        // See local entities
        var entityIDs = Entities.findEntitiesByType("Shape", actor.position, AVATAR_DIST_MAX);
        for (i = 0; i < entityIDs.length; i++) {
            var entity = Entities.getEntityProperties(entityIDs[i]);
            if (Object.keys(entity).length === 0) continue; // entity doesn't exist?
            if (entity.locked || entityIDs[i]==this.actorID) continue;

            var dist = Vec3.distance(actor.position, entity.position);
            if (dist > AVATAR_DIST_MAX) continue;
            var rads = Vec3.getAngle(actor.position, entity.position);
            // print("GymAC.update Entities added to obs: " + dist);

            if (dist < DIST_MAX*2) rwd += 0.001;

            var dist_center = Vec3.distance(POS_ZERO, entity.position);
            if (dist_center < DIST_MAX) rwd += (scoring)?0.03:0.01;

            var pose = getPose(dist, rads);
            pose.forEach(function(e){ obs[obsi++] = e; });
            if (obsi > obs_size) break; //max observation size
        }
    }

    // Entities.editEntity(this.actorID, { userData:JSON.stringify({ reward:rwd, observation:obs }) });
    if (rwd > 4.0) this.environment.info.saveart = true;
    this.environment.reward = rwd;
    this.environment.observation = obs;
}

function getPose(dist, rads) {
    if (isNaN(dist) || dist < 0.0) dist = 0.0;
    var near = (dist > AVATAR_DIST_NEAR) ? 255 : dist/AVATAR_DIST_NEAR*255;
    var far = dist/AVATAR_DIST_MAX*255;
    // print("GymAC.update rads: " + rads);
    if (isNaN(rads)) rads = 0.0;
    var angle = rads/Math.PI*255;
    if (angle > 255) angle = 255;
    var pose = [];
    pose[0] = 255 - parseInt(near);
    pose[1] = 255 - parseInt(far);
    pose[2] = parseInt(angle);
    return pose;
}

// function handleGymAgentChange(_agent) {
//     print("GymAC.handleGymAgentChange _agent: " + _agent);
//     this.agent = _agent;

//     // TODO send gym action_space and observation_space vars to agent when first connecting

//     // action_space = spaces.Discrete(18)
//     // observation_space = spaces.Box(low=0, high=255, shape=(4,4,3), dtype=np.uint8)
//     // message = {
//     //     action_space: {low: [0.0, 0.0, 0.0], high: [1.0, 1.0, 1.0], type:"float32"},
//     //     observation_space: {low: [-1.0, -1.0, -1.0, -1.0], high: [1.0, 1.0, 1.0, 1.0], type:"float32"},
//     // };
//     // Gym.sendGymMessage(this.agent, message);
// }

function handleGymMessage(_message) {
    // print("GymAC.handleGymMessage agent: " + this.agent);
    // print("GymAC.handleGymMessage _message: "+JSON.stringify(_message));

    // if (_message.agent != this.agent) return; //ignore other agents

    // Execute action
    var action = _message.message.action[0];
    // print("GymAC.handleGymMessage: action: "+JSON.stringify(action));

    var actor = Entities.getEntityProperties(this.actorID);
    if (Object.keys(actor).length !== 0) { // actor does not exist

        // var scoring = (Date.now() < this.environment.info.scoring);

        if (action == "reset") {
            var createplatform = false;
            // print("GymAC.handleGymMessage: RESET!");

            if (this.environment.info.paintparent) {
                // print("GymAC.handleGymMessage: saveart: "+this.environment.info.saveart);
                if (Entities.isAddedEntity(this.environment.info.paintparent)) {
                    if (this.environment.info.saveart) {
                        this.environment.info.saveart = false;
                        // move all old up
                        for (t = 0; t < this.environment.info.artindex; t++) {
                            var platformIDs = Entities.findEntitiesByName("DerpArt"+t, POS_ZERO, AVATAR_DIST_MAX, false);
                            for (i = 0; i < platformIDs.length; i++) {
                                var platform = Entities.getEntityProperties(platformIDs[i], ["position"]);
                                var newpos = Vec3.sum(platform.position, { x: 0, y: 1.0, z: 0 });
                                newpos = Vec3.multiplyQbyV(ROT_PLATFORM_ART, newpos);
                                Entities.editEntity(platformIDs[i], { position: newpos });
                            }
                        }
                        // move
                        Entities.editEntity(this.environment.info.paintparent, { position: { x: 0, y: -0.4, z: 8.0 } });
    
                        this.environment.info.artindex += 1;
                        // print("GymAC.handleGymMessage: SAVE! " + this.environment.info.paintparent);
                    } else {
                        Entities.deleteEntity(this.environment.info.paintparent);
                        this.environment.info.numobj = 0;
                        // print("GymAC.handleGymMessage: DELETE! " + this.environment.info.paintparent);
                    }
                    createplatform = true;
                }

            } else createplatform = true;

            if (createplatform) {
                // this.environment.info.paintparent = Entities.addEntity({
                //     // visible: false,
                //     name: "DerpArt"+this.environment.info.artindex,
                //     type: "Shape",
                //     shape: "Cylinder",
                //     position: { x: 0.0, y: 0.0, z: 0.0 },
                //     dimensions: { x: 5.0, y: 0.1, z: 5.0 },
                //     color: { red: 0, green: 128, blue: 255 },
                //     alpha: 0.5,
                //     lifetime: 120,
                // });
                this.environment.info.paintparent = Entities.addEntity({
                    // visible: false,
                    name: "DerpArt"+this.environment.info.artindex,
                    type: "Model",
                    modelURL: "https://xaotica.s3-us-west-1.amazonaws.com/ArtQ/Entities/SmallCircle.fbx",
                    position: { x: 0.0, y: 0.0, z: 0.0 },
                    dimensions: { x: 5, y: 0.18750138580799103, z: 5 },
                    grab: { grabbable: false },
                    // lifetime: 180,
                });
                // print("GymAC.handleGymMessage: CREATED! DerpArt"+this.environment.info.artindex + " " + this.environment.info.paintparent);
            }

            // SHAPES[Math.floor(Math.random()*SHAPES.length)]
            // Entities.editEntity(this.actorID, { shape: SHAPE_ZERO, dimensions: { x: 0.5, y: 0.5, z: 0.5 }, color: COLOR_ZERO, alpha: 1.0 });
            // Entities.editEntity(this.actorID, { visible: true });
            Entities.editEntity(this.actorID, { position: POS_ZERO, shape: SHAPE_ZERO, dimensions: { x: 0.5, y: 0.5, z: 0.5 }, alpha: 1.0 });
        }


        if (!this.environment.info.scoring) {
            
            if (action == 0) {
                Entities.editEntity(this.actorID, { position: Vec3.sum(actor.position, MOVE_Y) });
            } else if (action == 1) {
                Entities.editEntity(this.actorID, { position: Vec3.sum(actor.position, MOVE_X) });
            } else if (action == 2) {
                Entities.editEntity(this.actorID, { position: Vec3.sum(actor.position, MOVE_Z) });
            } else if (action == 3) {
                Entities.editEntity(this.actorID, { position: Vec3.subtract(actor.position, MOVE_Y) });
            } else if (action == 4) {
                Entities.editEntity(this.actorID, { position: Vec3.subtract(actor.position, MOVE_X) });
            } else if (action == 5) {
                Entities.editEntity(this.actorID, { position: Vec3.subtract(actor.position, MOVE_Z) });
    
            } else if (action == 6) {
                Entities.editEntity(this.actorID, { position: Vec3.sum(actor.position, MOVE_FAST_X) });
            } else if (action == 7) {
                Entities.editEntity(this.actorID, { position: Vec3.sum(actor.position, MOVE_FAST_Z) });
            } else if (action == 8) {
                Entities.editEntity(this.actorID, { position: Vec3.subtract(actor.position, MOVE_FAST_X) });
            } else if (action == 9) {
                Entities.editEntity(this.actorID, { position: Vec3.subtract(actor.position, MOVE_FAST_Z) });
                
            } else if (action == 10) {
                Entities.editEntity(this.actorID, { rotation: Quat.multiply(ROT_Xl, actor.rotation) });
            } else if (action == 11) {
                Entities.editEntity(this.actorID, { rotation: Quat.multiply(ROT_XR, actor.rotation) });
            } else if (action == 12) {
                Entities.editEntity(this.actorID, { rotation: Quat.multiply(ROT_Yl, actor.rotation) });
            } else if (action == 13) {
                Entities.editEntity(this.actorID, { rotation: Quat.multiply(ROT_YR, actor.rotation) });
            } else if (action == 14) {
                Entities.editEntity(this.actorID, { rotation: Quat.multiply(ROT_Zl, actor.rotation) });
            } else if (action == 15) {
                Entities.editEntity(this.actorID, { rotation: Quat.multiply(ROT_ZR, actor.rotation) });
            
            } else if (this.environment.info.paintparent && Entities.isAddedEntity(this.environment.info.paintparent)) {

                if (action == 16) {
                    if (this.environment.info.numobj < MAX_OBJ_PER_ART) {
                        // create object
                        Entities.addEntity({
                            parentID: this.environment.info.paintparent,
                            type: "Shape",
                            shape: actor.shape,
                            // localPosition: Vec3.sum(actor.position, Vec3.multiplyQbyV(actor.rotation, { x: 0, y: 0, z: 0 })),
                            // localPosition: actor.position,
                            // localRotation: actor.rotation,
                            position: actor.position,
                            rotation: actor.rotation,
                            dimensions: { x: 0.25, y: 0.25, z: 0.25 },
                            color: actor.color,
                            alpha: 1.0,
                        });
                        this.environment.info.numobj += 1;
                    }
                // } else if (action == 17) {
                //     // delete object
                // } else if (action == 17) {
                //     // pickup object
                // } else if (action == 17) {
                //     // drop object

                } else if (action == 23) {
                    var i = SHAPES.indexOf(actor.shape);
                    if (i >= 0 && i < SHAPES.length-1) Entities.editEntity(this.actorID, { shape: SHAPES[i+1] });
                } else if (action == 24) {
                    var i = SHAPES.indexOf(actor.shape);
                    if (i >= 1) Entities.editEntity(this.actorID, { shape: SHAPES[i-1] });

                } else if (action == 25) {
                    var newcolor = (actor.color.red >= 247) ? 255 : actor.color.red + 8;
                    Entities.editEntity(this.actorID, { color: { red: newcolor, green: actor.color.green, blue: actor.color.blue }});
                } else if (action == 26) {
                    var newcolor = (actor.color.green >= 247) ? 255 : actor.color.green + 8;
                    Entities.editEntity(this.actorID, { color: { red: actor.color.red, green: newcolor, blue: actor.color.blue }});
                } else if (action == 27) {
                    var newcolor = (actor.color.blue >= 247) ? 255 : actor.color.blue + 8;
                    Entities.editEntity(this.actorID, { color: { red: actor.color.red, green: actor.color.green, blue: newcolor }});

                } else if (action == 28) {
                    var newcolor = (actor.color.red <= 8) ? 0 : actor.color.red - 8;
                    Entities.editEntity(this.actorID, { color: { red: newcolor, green: actor.color.green, blue: actor.color.blue }});
                } else if (action == 29) {
                    var newcolor = (actor.color.green <= 8) ? 0 : actor.color.green - 8;
                    Entities.editEntity(this.actorID, { color: { red: actor.color.red, green: newcolor, blue: actor.color.blue }});
                } else if (action == 30) {
                    var newcolor = (actor.color.blue <= 8) ? 0 : actor.color.blue - 8;
                    Entities.editEntity(this.actorID, { color: { red: actor.color.red, green: actor.color.green, blue: newcolor }});

                }
    
            }

            if (Vec3.length(actor.position) > AVATAR_DIST_MAX) Entities.editEntity(this.actorID, { position: Vec3.subtract(actor.position, Vec3.normalize(actor.position)) });

        }


        if (action == 31) {
            // wait for scoring
            // print("GymAC.handleGymMessage: scoring: "+this.environment.info.scoring);
            // this.environment.info.scoring = Date.now() + SCORE_WAIT;
            // this.environment.info.scoring = !this.environment.info.scoring;
            // if (!this.environment.info.scoring) this.environment.done = true;
            this.environment.info.scoringwait++;
            if (this.environment.info.scoringwait > 15) {
                this.environment.info.scoringwait = 0;
                this.environment.info.scoring = !this.environment.info.scoring;
                if (!this.environment.info.scoring) this.environment.done = true;
                else {
                    // Entities.editEntity(this.actorID, { shape: SHAPE_ZERO, dimensions: { x: 1.0, y: 1.0, z: 1.0 }, color: COLOR_PINK, alpha: 0.5 });
                    Entities.editEntity(this.actorID, { shape: SHAPE_ZERO, dimensions: { x: 1.0, y: 1.0, z: 1.0 }, alpha: 0.5 });
                    // Entities.editEntity(this.actorID, { visible: false });
                }
            }
        }
    

        // if (action == 28) {
        //     Entities.editEntity(this.actorID, { color: { red: (actor.color.red==255)?0:255, green: actor.color.green, blue: actor.color.blue }});
        // } else if (action == 29) {
        //     Entities.editEntity(this.actorID, { color: { red: actor.color.red, green: (actor.color.green==255)?0:255, blue: actor.color.blue }});
        // } else if (action == 30) {
        //     Entities.editEntity(this.actorID, { color: { red: actor.color.red, green: actor.color.green, blue: (actor.color.blue==255)?0:255 }});

        // else {
        //     // Entities.editEntity(this.actorID, { color: { red: 255 * action[0], green: 255 * action[1], blue: 255 * action[2]} });
        //     // Entities.editEntity(this.actorID, { color: { red: 10 * action, green: 10 * action, blue: 10 * action} });
        // }


    }

    // Return environment
    // this.update(0);
    Gym.sendGymMessage(this.agent, this.environment);
    if (this.environment.done) this.environment.done = false; // make sure to send, reset after send
}

function unload() {
    Script.update.disconnect(update);
    Gym.onGymMessage.disconnect(handleGymMessage);
    // Gym.onGymAgentChange.disconnect(handleGymAgentChange);
}

// function platformPeopleClick(entityID, event) {
//     print("Mouse pressed on entity: " + JSON.stringify(event));
// }
// Script.addEventHandler(platformPeople, "mousePressOnEntity", platformPeopleClick);

// Gym.onGymAgentChange.connect(handleGymAgentChange);
Gym.onGymMessage.connect(handleGymMessage);
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


