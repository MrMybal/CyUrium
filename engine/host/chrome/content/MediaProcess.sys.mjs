// SPDX-FileCopyrightText: 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
import {WebRTCChild} from "resource:///actors/WebRTCChild.sys.mjs";
export class CyUriumMediaChild extends JSProcessActorChild {
  observe(subject,topic,data){WebRTCChild.observe(subject,topic,data);}
}
