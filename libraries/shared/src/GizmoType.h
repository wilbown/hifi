//
//  Created by Sam Gondelman on 1/22/19.
//  Copyright 2019 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_GizmoType_h
#define hifi_GizmoType_h

#include "QString"

/**jsdoc
 * <p>Controls how the Gizmo behaves and renders</p>
 * <table>
 *   <thead>
 *     <tr><th>Value</th><th>Description</th></tr>
 *   </thead>
 *   <tbody>
 *     <tr><td><code>ring</code></td><td>A ring gizmo.</td></tr>
 *   </tbody>
 * </table>
 * @typedef {string} GizmoType
 */

enum GizmoType {
    RING = 0,
};

class GizmoTypeHelpers {
public:
    static QString getNameForGizmoType(GizmoType mode);
};

#endif // hifi_GizmoType_h

