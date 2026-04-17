"""
Federated Learning Aggregation Server
"""
import json
import numpy as np
from flask import Flask, request, jsonify
from collections import defaultdict
import threading

app = Flask(__name__)

INPUT_DIM = 9
HIDDEN1 = 16
HIDDEN2 = 8
OUTPUT_DIM = 4
TOTAL_WEIGHTS = (INPUT_DIM * HIDDEN1 + HIDDEN1) + (HIDDEN1 * HIDDEN2 + HIDDEN2) + (HIDDEN2 * OUTPUT_DIM + OUTPUT_DIM)


def init_weights():
    rng = np.random.default_rng(42)
    shapes = [
        (INPUT_DIM, HIDDEN1), (HIDDEN1,),
        (HIDDEN1, HIDDEN2), (HIDDEN2,),
        (HIDDEN2, OUTPUT_DIM), (OUTPUT_DIM,),
    ]
    weights = []
    for s in shapes:
        if len(s) == 2:
            fan_in, fan_out = s
            limit = np.sqrt(6 / (fan_in + fan_out))
            weights.append(rng.uniform(-limit, limit, s).tolist())
        else:
            weights.append(np.zeros(s).tolist())
    return weights


lock = threading.Lock()
global_weights = init_weights()
round_number = 0
client_updates = defaultdict(list)
MIN_CLIENTS = 1


def federated_average(updates):
    total_samples = sum(n for _, n in updates)
    new_weights = []
    for layer_idx in range(len(updates[0][0])):
        layer_avg = np.zeros_like(np.array(updates[0][0][layer_idx], dtype=float))
        for w, n in updates:
            layer_avg += np.array(w[layer_idx], dtype=float) * (n / total_samples)
        new_weights.append(layer_avg.tolist())
    return new_weights


@app.route("/model", methods=["GET"])
def get_model():
    with lock:
        return jsonify({"round": round_number, "weights": global_weights})


@app.route("/update", methods=["POST"])
def receive_update():
    global global_weights, round_number
    data = request.get_json(force=True)
    if not data or "weights" not in data:
        return jsonify({"error": "missing weights"}), 400

    client_id = data.get("client_id", "unknown")
    client_round = data.get("round", 0)
    weights = data["weights"]
    n_samples = data.get("n_samples", 1)

    flat = []
    for layer in weights:
        if isinstance(layer, list):
            flat.extend(layer)
        else:
            flat.append(layer)
    if len(flat) != TOTAL_WEIGHTS:
        print(f"[ERROR] {client_id} sent {len(flat)} weights, expected {TOTAL_WEIGHTS}")
        return jsonify({"error": f"wrong weight count: {len(flat)}"}), 400

    print(f"[Round {client_round}] Update from {client_id} ({n_samples} samples)")

    with lock:
        client_updates[client_round].append((weights, n_samples))
        updates_this_round = len(client_updates[client_round])

    if updates_this_round >= MIN_CLIENTS:
        with lock:
            updates = client_updates.pop(client_round, [])
            if updates:
                global_weights = federated_average(updates)
                round_number += 1
                print(f"Aggregated round {client_round} -> new global round {round_number}")

    return jsonify({"status": "ok", "global_round": round_number})


@app.route("/status", methods=["GET"])
def status():
    with lock:
        pending = {k: len(v) for k, v in client_updates.items()}
    return jsonify({
        "global_round": round_number,
        "pending_updates": pending,
        "min_clients": MIN_CLIENTS,
        "total_weights": TOTAL_WEIGHTS,
    })


if __name__ == "__main__":
    print("=" * 50)
    print("Federated Learning Server")
    print(f"  Model: {INPUT_DIM}->{HIDDEN1}->{HIDDEN2}->{OUTPUT_DIM}")
    print(f"  Total weights: {TOTAL_WEIGHTS}")
    print(f"  Waiting for {MIN_CLIENTS} clients per round")
    print("=" * 50)
    app.run(host="0.0.0.0", port=5000, debug=False)
