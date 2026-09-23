# Shot logging via Home Assistant, InfluxDB and Grafana

The display uploads every finished shot as JSON to a Home Assistant webhook
(configured on the device's setup portal, `status.html` -> "Shot logging").
This is reference configuration for the Home Assistant side - it is not
applied automatically, since this session has no access to your running
instance. Copy the relevant parts into your own `configuration.yaml`/
`automations.yaml` and adjust entity/measurement names to taste.

## What the device sends

```json
{
  "shot_id": "2026-09-23T07:15:32Z",
  "duration_s": 27.5,
  "final_weight_g": 38.2,
  "avg_flow_g_per_s": 1.39,
  "target_temp_label": "94°C",
  "samples": [
    { "t_s": 0.0, "weight_g": 0.0, "flow_g_per_s": 0.0 },
    { "t_s": 0.5, "weight_g": 0.8, "flow_g_per_s": 1.6 }
  ]
}
```

No pressure and no temperature-over-time curve: the display has no pressure
sensor, and the La Marzocco API only exposes a target/setpoint temperature,
not a continuous reading. `target_temp_label` is a snapshot of that setpoint
at the moment the shot finished.

## 1. Webhook -> InfluxDB (the curve)

```yaml
# automations.yaml
- alias: "Shot: log to InfluxDB"
  trigger:
    - platform: webhook
      webhook_id: shot_upload   # matches the URL configured on the device
      local_only: true          # local network only
  action:
    - service: input_text.set_value
      target:
        entity_id: input_text.last_shot_id
      data:
        value: "{{ trigger.json.shot_id }}"
    - service: influxdb.write
      data:
        measurement: shot_curve
        tags:
          shot_id: "{{ trigger.json.shot_id }}"
        fields:
          duration_s: "{{ trigger.json.duration_s }}"
          final_weight_g: "{{ trigger.json.final_weight_g }}"
          avg_flow_g_per_s: "{{ trigger.json.avg_flow_g_per_s }}"
          target_temp_label: "{{ trigger.json.target_temp_label | default('') }}"
    - repeat:
        for_each: "{{ trigger.json.samples }}"
        sequence:
          - service: influxdb.write
            data:
              measurement: shot_sample
              tags:
                shot_id: "{{ trigger.json.shot_id }}"
              fields:
                t_s: "{{ repeat.item.t_s }}"
                weight_g: "{{ repeat.item.weight_g }}"
                flow_g_per_s: "{{ repeat.item.flow_g_per_s }}"
```

Requires the built-in [InfluxDB integration](https://www.home-assistant.io/integrations/influxdb/)
configured for write access to your InfluxDB instance.

## 2. Metadata form (bean, grind, rating, notes)

Grafana has no structured input fields, so the enrichment happens in a small
Home Assistant dashboard instead, referencing the shot just logged:

```yaml
# configuration.yaml
input_text:
  last_shot_id:
    name: Last shot ID
  shot_notes:
    name: Notes

input_select:
  shot_bean:
    name: Bean
    options:
      - "Bean A"
      - "Bean B"

input_number:
  shot_grind:
    name: Grind setting
    min: 0
    max: 50
    step: 0.5
  shot_rating:
    name: Rating
    min: 1
    max: 10
    step: 1

script:
  save_shot_metadata:
    sequence:
      - service: influxdb.write
        data:
          measurement: shot_metadata
          tags:
            shot_id: "{{ states('input_text.last_shot_id') }}"
          fields:
            bean: "{{ states('input_select.shot_bean') }}"
            grind: "{{ states('input_number.shot_grind') }}"
            rating: "{{ states('input_number.shot_rating') }}"
            notes: "{{ states('input_text.shot_notes') }}"
```

Add a Lovelace card with `input_select.shot_bean`, `input_number.shot_grind`,
`input_number.shot_rating`, `input_text.shot_notes` and a button calling
`script.save_shot_metadata`, so a shot can be annotated right after pulling it
(or later - `input_text.last_shot_id` can be edited by hand to annotate an
older shot).

## 3. Grafana dashboard

- A `shot_id` template variable, sourced from a Flux/InfluxQL query over the
  `shot_curve` measurement's `shot_id` tag (sorted descending, so the newest
  shot is first).
- A time-series panel over `shot_sample` filtered to the selected `shot_id`,
  plotting `weight_g` and `flow_g_per_s` against `t_s`.
- A table panel joining `shot_curve` and `shot_metadata` on `shot_id`, listing
  past shots with duration, weight, bean, grind and rating - your shot
  history / journal.

## Notes

- The device sends over plain local HTTP (no TLS), matching the `local_only`
  webhook trigger above. Do not expose this webhook to the internet.
- A shot is uploaded once; a failed upload (Home Assistant unreachable,
  restarting, ...) is not retried or buffered on the device. If Home
  Assistant on the Pi crashes often, shots pulled during that window are
  lost - worth fixing independently of this logging setup.
