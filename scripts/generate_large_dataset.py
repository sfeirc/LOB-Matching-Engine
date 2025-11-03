#!/usr/bin/env python3
"""
Generate a large CSV dataset for testing order book performance.
"""

import random
import sys
import os
import time
from datetime import datetime

def generate_dataset(num_messages, output_file, seed=42):
    """
    Generate a realistic order book dataset.
    
    Args:
        num_messages: Total number of messages to generate
        output_file: Output CSV file path
        seed: Random seed for reproducibility
    """
    random.seed(seed)
    
    # Configuration
    base_price = 100000  # Base price in ticks
    price_range = 500    # Price range: base_price to base_price + price_range
    min_qty = 1
    max_qty = 1000
    
    # Track active orders for realistic cancels
    active_orders = {}  # order_id -> (side, price)
    next_order_id = 1
    
    # Start timestamp (nanoseconds)
    start_ts = 1693526400000000000
    current_ts = start_ts
    
    # Message type weights (NewLimit: 70%, NewMarket: 20%, Cancel: 10%)
    msg_type_weights = ["NewLimit"] * 70 + ["NewMarket"] * 20 + ["Cancel"] * 10
    
    print(f"Generating {num_messages:,} messages...")
    start_time = time.time()
    
    with open(output_file, 'w') as f:
        # Write header
        f.write("# ts_ns,MsgType,Side,OrderId,Price,Qty\n")
        
        messages_generated = 0
        last_progress = 0
        
        for i in range(num_messages):
            # Progress reporting
            progress = (i * 100) // num_messages
            if progress >= last_progress + 10:
                print(f"Progress: {progress}% ({i:,} messages)", end='\r')
                last_progress = progress
            
            # Increment timestamp (1-1000 microseconds per message)
            current_ts += random.randint(1000, 1000000)
            
            # Choose message type
            msg_type = random.choice(msg_type_weights)
            
            # Generate cancel only if we have active orders
            if msg_type == "Cancel" and len(active_orders) > 0:
                # Cancel a random active order
                order_id = random.choice(list(active_orders.keys()))
                side, price = active_orders.pop(order_id)
                
                f.write(f"{current_ts},Cancel,{side},{order_id},0,0\n")
                messages_generated += 1
                
            elif msg_type == "NewMarket":
                # Market order
                side = random.choice(["Buy", "Sell"])
                order_id = next_order_id
                next_order_id += 1
                qty = random.randint(min_qty, max_qty)
                
                f.write(f"{current_ts},NewMarket,{side},{order_id},0,{qty}\n")
                messages_generated += 1
                
            else:  # NewLimit
                # Limit order
                side = random.choice(["Buy", "Sell"])
                order_id = next_order_id
                next_order_id += 1
                price = base_price + random.randint(0, price_range)
                qty = random.randint(min_qty, max_qty)
                
                # Store for potential cancel
                active_orders[order_id] = (side, price)
                
                # Remove some old orders to keep memory reasonable
                if len(active_orders) > 100000:
                    # Remove 10% of oldest orders
                    keys_to_remove = random.sample(list(active_orders.keys()), len(active_orders) // 10)
                    for key in keys_to_remove:
                        active_orders.pop(key, None)
                
                f.write(f"{current_ts},NewLimit,{side},{order_id},{price},{qty}\n")
                messages_generated += 1
    
    elapsed = time.time() - start_time
    print(f"\nGenerated {messages_generated:,} messages in {elapsed:.2f} seconds")
    print(f"Output file: {output_file}")
    print(f"File size: {os.path.getsize(output_file) / (1024*1024):.2f} MB")


if __name__ == "__main__":
    # Default: 1 million messages, but allow override
    num_messages = 1_000_000
    if len(sys.argv) > 1:
        try:
            num_messages = int(sys.argv[1])
        except ValueError:
            print("Usage: python generate_large_dataset.py [num_messages]")
            sys.exit(1)
    
    # Create data directory if it doesn't exist
    os.makedirs("data", exist_ok=True)
    
    output_file = f"data/large_dataset_{num_messages//1000}k.csv"
    
    generate_dataset(num_messages, output_file)

