# Getting Started with Create React App

This project was bootstrapped with [Create React App](https://github.com/facebook/create-react-app).

## Available Scripts

In the project directory, you can run:

### `npm start`

Runs the app in the development mode.\
Open [http://localhost:3000](http://localhost:3000) to view it in your browser.

The page will reload when you make changes.\
You may also see any lint errors in the console.

### `npm test`

Launches the test runner in the interactive watch mode.\
See the section about [running tests](https://facebook.github.io/create-react-app/docs/running-tests) for more information.

## Gardenia admin dashboard

Development uses `REACT_APP_API_BASE_URL=http://localhost:8080` from the
committed `.env.development` file. Change it there if the backend runs at a
different address, then restart `npm start`. For production, set
`REACT_APP_API_BASE_URL` to the public backend URL before running `npm run build`;
Create React App embeds this value at build time. The backend must allow the
frontend origin through CORS.

Sign-in uses `POST /api/auth/login`; the authenticated home page is restricted
to accounts whose response role is `ADMIN`.

The dashboard queries:

- `GET /api/admin/statistics/environmental?deviceId={id}&range={24h|7d|30d}`
- `GET /api/admin/statistics/water-level?deviceId={id}&range={24h|7d|30d}`

Both endpoints return arrays. The environmental array accepts
`timestamp`/`recordedAt` and soil moisture, humidity, and temperature values;
the water-level array accepts timestamps and `waterLevel`.

The **Thiết bị IoT** page lists devices, creates a new device, and registers an
existing device to a user:

- `GET /api/admin/devices` — list all devices (administrator token required).
- `POST /api/admin/devices` — create a device using its MAC address (administrator token required).
- `POST /api/devices/{id}/register`
- Request body: `{ "userId": 12 }`
- Requires an administrator token. The device must already exist and the
  target account must have the `USER` role.

Device creation accepts `{ "id": "AA:BB:CC:DD:EE:FF", "deviceName": "Garden pump", "hardwareVersion": "ESP32", "firmwareVersion": "1.0.0" }`. Only `id` is required. Newly created devices have status `registered`; assigning an owner changes the status to `provisioned`, and the device changes to `active` after its first MQTT status/telemetry report. The create response includes `ble_secret_key`, `mqtt_username`, and `mqtt_password` once for provisioning. The page displays those values after creation; save them before leaving because the listing API never returns credentials. The MQTT username is derived from the normalized MAC (for example `dev_AABBCCDDEEFF`); both MQTT credential fields fit the database limit of 55 characters. Lifecycle statuses shown in the device table include registered, provisioned, active, offline, fault, maintenance, and decommissioned.

The device listing returns `id`, `device_name`, `status`, `hardware_version`,
`firmware_version`, and `last_active_at`. There is no administrator API for
listing users yet, so the page accepts the target user ID directly and shows
the backend response after registration.

### `npm run build`

Builds the app for production to the `build` folder.\
It correctly bundles React in production mode and optimizes the build for the best performance.

The build is minified and the filenames include the hashes.\
Your app is ready to be deployed!

See the section about [deployment](https://facebook.github.io/create-react-app/docs/deployment) for more information.

### `npm run eject`

**Note: this is a one-way operation. Once you `eject`, you can't go back!**

If you aren't satisfied with the build tool and configuration choices, you can `eject` at any time. This command will remove the single build dependency from your project.

Instead, it will copy all the configuration files and the transitive dependencies (webpack, Babel, ESLint, etc) right into your project so you have full control over them. All of the commands except `eject` will still work, but they will point to the copied scripts so you can tweak them. At this point you're on your own.

You don't have to ever use `eject`. The curated feature set is suitable for small and middle deployments, and you shouldn't feel obligated to use this feature. However we understand that this tool wouldn't be useful if you couldn't customize it when you are ready for it.

## Learn More

You can learn more in the [Create React App documentation](https://facebook.github.io/create-react-app/docs/getting-started).

To learn React, check out the [React documentation](https://reactjs.org/).

### Code Splitting

This section has moved here: [https://facebook.github.io/create-react-app/docs/code-splitting](https://facebook.github.io/create-react-app/docs/code-splitting)

### Analyzing the Bundle Size

This section has moved here: [https://facebook.github.io/create-react-app/docs/analyzing-the-bundle-size](https://facebook.github.io/create-react-app/docs/analyzing-the-bundle-size)

### Making a Progressive Web App

This section has moved here: [https://facebook.github.io/create-react-app/docs/making-a-progressive-web-app](https://facebook.github.io/create-react-app/docs/making-a-progressive-web-app)

### Advanced Configuration

This section has moved here: [https://facebook.github.io/create-react-app/docs/advanced-configuration](https://facebook.github.io/create-react-app/docs/advanced-configuration)

### Deployment

This section has moved here: [https://facebook.github.io/create-react-app/docs/deployment](https://facebook.github.io/create-react-app/docs/deployment)

### `npm run build` fails to minify

This section has moved here: [https://facebook.github.io/create-react-app/docs/troubleshooting#npm-run-build-fails-to-minify](https://facebook.github.io/create-react-app/docs/troubleshooting#npm-run-build-fails-to-minify)
